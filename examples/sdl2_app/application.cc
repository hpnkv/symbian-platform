// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include "sdl2_app/application.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

#include "sdl2_app/arkanoid_adapter.h"
#include "sdl2_app/game.h"
#include "sdl2_app/renderer.h"
#include "symbian/api/display/display.h"
#include "symbian/api/media/game_feedback.h"
#include "symbian/api/time/frame_pacer.h"
#include "symbian/api/time/sleep.h"

namespace arkanoid {
namespace {
extern "C" int symbian_sdl2_c_api_probe(void);
constexpr int kStepMs = 16;
}  // namespace

int Run() {
  if (!symbian_sdl2_c_api_probe()) {
    return 5;
  }
#ifdef SYMBIAN_ARKANOID_SDL3
  auto session = symbian::sdl3::Session::Start(SDL_INIT_VIDEO);
#else
  auto session = symbian::sdl2::Session::Start(SDL_INIT_VIDEO);
#endif
  if (!session.ok()) {
    return 1;
  }
#ifdef SYMBIAN_ARKANOID_SDL3
  auto window =
      symbian::sdl3::Window::Create("Bounce Style Arkanoid", 640, 360, 0);
#else
  auto window = symbian::sdl2::Window::Create("Bounce Style Arkanoid", 640, 360,
                                              SDL_WINDOW_SHOWN);
#endif
  if (!window.ok()) {
    return 2;
  }
  std::string gpu_fallback_reason;
#ifdef SYMBIAN_ARKANOID_SDL3
#ifdef SYMBIAN_ARKANOID_GPU
  auto renderer = symbian::sdl3::Renderer::Create(window->get(), "opengles2");
  if (!renderer.ok()) {
    gpu_fallback_reason = SDL_GetError();
    renderer = symbian::sdl3::Renderer::Create(window->get());
  }
  if (renderer.ok()) {
    SDL_SetRenderVSync(renderer->get(), 1);
  }
#else
  auto renderer = symbian::sdl3::Renderer::Create(window->get());
#endif
#else
#ifdef SYMBIAN_ARKANOID_GPU
  auto renderer = symbian::sdl2::Renderer::Create(
      window->get(), SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!renderer.ok()) {
    gpu_fallback_reason = SDL_GetError();
    renderer =
        symbian::sdl2::Renderer::Create(window->get(), SDL_RENDERER_SOFTWARE);
  }
#else
  auto renderer =
      symbian::sdl2::Renderer::Create(window->get(), SDL_RENDERER_SOFTWARE);
#endif
#endif
  if (!renderer.ok()) {
    return 3;
  }
  bool gpu_active = false;
#ifdef SYMBIAN_ARKANOID_SDL3
  const char* absl_nullable renderer_name =
      SDL_GetRendererName(renderer->get());
  gpu_active = renderer_name != nullptr &&
               std::string_view(renderer_name) == "opengles2";
#else
  SDL_RendererInfo renderer_info = {};
  gpu_active = SDL_GetRendererInfo(renderer->get(), &renderer_info) == 0 &&
               (renderer_info.flags & SDL_RENDERER_ACCELERATED) != 0;
#endif
  int width = 0;
  int height = 0;
  SDL_GetWindowSize(window->get(), &width, &height);
  if (width < 240 || height < 200) {
    return 4;
  }
  const auto touchscreen_presence =
      symbian::api::display::ReadTouchscreenPresence();
  // An unknown HAL result keeps the on-screen control available.
  const bool touchscreen =
      !touchscreen_presence.ok() || *touchscreen_presence;
  Game game(width, height, touchscreen);
  symbian::api::media::GameFeedback feedback;
  std::string vibration_error;
  arkanoid::GameRenderer game_renderer;
  // Present a complete frame before opening device media services. Some
  // firmware starts those servers synchronously, so they must not keep the
  // initial window white while the request is in flight.
  game_renderer.Draw(game, renderer->get(), 0, gpu_active, gpu_fallback_reason,
                     vibration_error);
  feedback.Start();
  std::uint32_t last_hits = 0;
  bool running = true;
  bool foreground = true;
  const std::uint32_t reported_rate = arkanoid::RefreshRateHz();
  symbian::api::time::FramePacer frame_pacer(
      reported_rate == 0 ? 60 : reported_rate);
  std::uint64_t previous = arkanoid::Ticks();
  std::uint64_t lag = 0;
  std::uint64_t fps_sample_start = previous;
  std::uint32_t frames_in_sample = 0;
  std::uint32_t frames_per_second = 0;
  while (running) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (arkanoid::Quit(event)) {
        running = false;
      }
      if (arkanoid::Backgrounded(event)) {
        foreground = false;
      }
      if (arkanoid::Foregrounded(event)) {
        foreground = true;
      }
      game.Handle(event);
    }
    if (game.exit_requested()) {
      running = false;
    }
    if (!running) {
      break;
    }
    if (!foreground) {
      feedback.SetMusicEnabled(false);
      symbian::api::time::SleepFor(std::chrono::milliseconds(80));
      previous = arkanoid::Ticks();
      lag = 0;
      frame_pacer.Reset();
      fps_sample_start = previous;
      frames_in_sample = 0;
      continue;
    }
    const std::uint64_t now = arkanoid::Ticks();
    lag += std::min<std::uint64_t>(now - previous, 64);
    previous = now;
    while (lag >= kStepMs) {
      game.Step();
      lag -= kStepMs;
    }
    if (game.hits() != last_hits) {
      feedback.Hit(now);
      last_hits = game.hits();
    }
    feedback.SetMusicEnabled(game.music_enabled());
    feedback.Pump(now);
    if (vibration_error.empty()) {
      const absl::Status vibration_status = feedback.vibration_status();
      if (!vibration_status.ok()) {
        vibration_error.assign(vibration_status.message().data(),
                               vibration_status.message().size());
      }
    }
    game_renderer.Draw(game, renderer->get(), frames_per_second, gpu_active,
                       gpu_fallback_reason, vibration_error);
    ++frames_in_sample;
    const std::uint64_t sample_elapsed = arkanoid::Ticks() - fps_sample_start;
    if (sample_elapsed >= 1000) {
      frames_per_second = static_cast<std::uint32_t>(
          (static_cast<std::uint64_t>(frames_in_sample) * 1000 +
           sample_elapsed / 2) /
          sample_elapsed);
      fps_sample_start = arkanoid::Ticks();
      frames_in_sample = 0;
    }
    symbian::api::time::SleepFor(frame_pacer.NextDelayNanoseconds());
  }
  return 0;
}

}  // namespace arkanoid
