// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_SDL3_SDL3_H_
#define SYMBIAN_SDL3_SDL3_H_

#include <cstdint>
#include <utility>

#include <SDL3/SDL.h>
#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>

namespace symbian::sdl3 {

inline absl::Status SdlError() {
  return absl::InternalError(SDL_GetError());
}

class Session final {
 public:
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;

  Session(Session&& other) noexcept : flags_(std::exchange(other.flags_, 0)) {}

  ~Session() {
    if (flags_ != 0) {
      SDL_QuitSubSystem(flags_);
    }
  }

  static absl::StatusOr<Session> Start(std::uint32_t flags) {
    if (!SDL_InitSubSystem(flags)) {
      return SdlError();
    }
    return Session(flags);
  }

 private:
  explicit Session(std::uint32_t flags) : flags_(flags) {}

  std::uint32_t flags_;
};

class Window final {
 public:
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  Window(Window&& other) noexcept
      : value_(std::exchange(other.value_, nullptr)) {}

  ~Window() { SDL_DestroyWindow(value_); }

  static absl::StatusOr<Window> Create(const char* absl_nonnull title,
                                       int width, int height,
                                       std::uint64_t flags) {
    SDL_Window* absl_nullable value =
        SDL_CreateWindow(title, width, height, flags);
    if (value == nullptr) {
      return SdlError();
    }
    return Window(value);
  }

  SDL_Window* absl_nonnull get() const { return value_; }

 private:
  explicit Window(SDL_Window* absl_nonnull value) : value_(value) {}

  SDL_Window* absl_nullable value_;
};

class Renderer final {
 public:
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  Renderer(Renderer&& other) noexcept
      : value_(std::exchange(other.value_, nullptr)) {}

  Renderer& operator=(Renderer&& other) noexcept {
    if (this != &other) {
      SDL_DestroyRenderer(value_);
      value_ = std::exchange(other.value_, nullptr);
    }
    return *this;
  }

  ~Renderer() { SDL_DestroyRenderer(value_); }

  static absl::StatusOr<Renderer> Create(
      SDL_Window* absl_nonnull window,
      const char* absl_nonnull driver_name = "software") {
    SDL_Renderer* absl_nullable value = SDL_CreateRenderer(window, driver_name);
    if (value == nullptr) {
      return SdlError();
    }
    return Renderer(value);
  }

  SDL_Renderer* absl_nonnull get() const { return value_; }

 private:
  explicit Renderer(SDL_Renderer* absl_nonnull value) : value_(value) {}

  SDL_Renderer* absl_nullable value_;
};

}  // namespace symbian::sdl3

#endif  // SYMBIAN_SDL3_SDL3_H_
