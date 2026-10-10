#ifndef SYMBIAN_EXAMPLES_ARKANOID_ADAPTER_H_
#define SYMBIAN_EXAMPLES_ARKANOID_ADAPTER_H_

#include <cstdint>

#ifdef SYMBIAN_ARKANOID_SDL3
#include <SDL3/SDL.h>

#include "symbian/sdl3/sdl3.h"
#else
#include <SDL.h>

#include "symbian/sdl2/sdl2.h"
#endif

namespace arkanoid {

#ifdef SYMBIAN_ARKANOID_SDL3
inline bool Quit(const SDL_Event& event) {
  return event.type == SDL_EVENT_QUIT ||
         event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED;
}

inline bool KeyDown(const SDL_Event& event) {
  return event.type == SDL_EVENT_KEY_DOWN;
}

inline bool KeyUp(const SDL_Event& event) {
  return event.type == SDL_EVENT_KEY_UP;
}

inline bool MouseMotion(const SDL_Event& event) {
  return event.type == SDL_EVENT_MOUSE_MOTION;
}

inline bool MouseDown(const SDL_Event& event) {
  return event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
}

inline bool Backgrounded(const SDL_Event& event) {
  return event.type == SDL_EVENT_WINDOW_FOCUS_LOST ||
         event.type == SDL_EVENT_WINDOW_HIDDEN ||
         event.type == SDL_EVENT_WINDOW_MINIMIZED ||
         event.type == SDL_EVENT_WILL_ENTER_BACKGROUND ||
         event.type == SDL_EVENT_DID_ENTER_BACKGROUND;
}

inline bool Foregrounded(const SDL_Event& event) {
  return event.type == SDL_EVENT_WINDOW_FOCUS_GAINED ||
         event.type == SDL_EVENT_WINDOW_SHOWN ||
         event.type == SDL_EVENT_WINDOW_RESTORED ||
         event.type == SDL_EVENT_WILL_ENTER_FOREGROUND ||
         event.type == SDL_EVENT_DID_ENTER_FOREGROUND;
}

inline SDL_Keycode Key(const SDL_Event& event) {
  return event.key.key;
}

inline int MotionX(const SDL_Event& event) {
  return static_cast<int>(event.motion.x);
}

inline int MotionY(const SDL_Event& event) {
  return static_cast<int>(event.motion.y);
}

inline int ButtonX(const SDL_Event& event) {
  return static_cast<int>(event.button.x);
}

inline int ButtonY(const SDL_Event& event) {
  return static_cast<int>(event.button.y);
}

inline std::uint64_t Ticks() {
  return SDL_GetTicks();
}

inline std::uint32_t RefreshRateHz() {
  const SDL_DisplayMode* absl_nullable mode =
      SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
  return mode == nullptr || mode->refresh_rate <= 0
             ? 0
             : static_cast<std::uint32_t>(mode->refresh_rate);
}

inline void Fill(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
                 int height) {
  const SDL_FRect rectangle = {static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(width),
                               static_cast<float>(height)};
  SDL_RenderFillRect(renderer, &rectangle);
}

inline bool UpdateTexture(SDL_Texture* absl_nonnull texture,
                          const void* absl_nonnull pixels, int pitch) {
  return SDL_UpdateTexture(texture, nullptr, pixels, pitch);
}

inline void DrawTexture(SDL_Renderer* absl_nonnull renderer,
                        SDL_Texture* absl_nonnull texture, int source_x,
                        int source_y, int width, int height, int x, int y) {
  const SDL_FRect source = {
      static_cast<float>(source_x), static_cast<float>(source_y),
      static_cast<float>(width), static_cast<float>(height)};
  const SDL_FRect target = {static_cast<float>(x), static_cast<float>(y),
                            static_cast<float>(width),
                            static_cast<float>(height)};
  SDL_RenderTexture(renderer, texture, &source, &target);
}
#else
inline bool Quit(const SDL_Event& event) {
  return event.type == SDL_QUIT ||
         (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE);
}

inline bool KeyDown(const SDL_Event& event) {
  return event.type == SDL_KEYDOWN;
}

inline bool KeyUp(const SDL_Event& event) {
  return event.type == SDL_KEYUP;
}

inline bool MouseMotion(const SDL_Event& event) {
  return event.type == SDL_MOUSEMOTION;
}

inline bool MouseDown(const SDL_Event& event) {
  return event.type == SDL_MOUSEBUTTONDOWN;
}

inline bool Backgrounded(const SDL_Event& event) {
  return event.type == SDL_APP_WILLENTERBACKGROUND ||
         event.type == SDL_APP_DIDENTERBACKGROUND ||
         (event.type == SDL_WINDOWEVENT &&
          (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST ||
           event.window.event == SDL_WINDOWEVENT_HIDDEN ||
           event.window.event == SDL_WINDOWEVENT_MINIMIZED));
}

inline bool Foregrounded(const SDL_Event& event) {
  return event.type == SDL_APP_WILLENTERFOREGROUND ||
         event.type == SDL_APP_DIDENTERFOREGROUND ||
         (event.type == SDL_WINDOWEVENT &&
          (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
           event.window.event == SDL_WINDOWEVENT_SHOWN ||
           event.window.event == SDL_WINDOWEVENT_RESTORED));
}

inline SDL_Keycode Key(const SDL_Event& event) {
  return event.key.keysym.sym;
}

inline int MotionX(const SDL_Event& event) {
  return event.motion.x;
}

inline int MotionY(const SDL_Event& event) {
  return event.motion.y;
}

inline int ButtonX(const SDL_Event& event) {
  return event.button.x;
}

inline int ButtonY(const SDL_Event& event) {
  return event.button.y;
}

inline std::uint64_t Ticks() {
  return SDL_GetTicks64();
}

inline std::uint32_t RefreshRateHz() {
  SDL_DisplayMode mode = {};
  return SDL_GetCurrentDisplayMode(0, &mode) == 0 && mode.refresh_rate > 0
             ? static_cast<std::uint32_t>(mode.refresh_rate)
             : 0;
}

inline void Fill(SDL_Renderer* absl_nonnull renderer, int x, int y, int width,
                 int height) {
  const SDL_Rect rectangle = {x, y, width, height};
  SDL_RenderFillRect(renderer, &rectangle);
}

inline bool UpdateTexture(SDL_Texture* absl_nonnull texture,
                          const void* absl_nonnull pixels, int pitch) {
  return SDL_UpdateTexture(texture, nullptr, pixels, pitch) == 0;
}

inline void DrawTexture(SDL_Renderer* absl_nonnull renderer,
                        SDL_Texture* absl_nonnull texture, int source_x,
                        int source_y, int width, int height, int x, int y) {
  const SDL_Rect source = {source_x, source_y, width, height};
  const SDL_Rect target = {x, y, width, height};
  SDL_RenderCopy(renderer, texture, &source, &target);
}
#endif

}  // namespace arkanoid

#endif  // SYMBIAN_EXAMPLES_ARKANOID_ADAPTER_H_
