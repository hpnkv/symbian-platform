// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#ifndef SYMBIAN_SDL2_SDL2_H_
#define SYMBIAN_SDL2_SDL2_H_

#include <cstdint>
#include <memory>
#include <new>
#include <utility>

#include <SDL.h>
#include <absl/status/status_macros.h>
#include <absl/base/nullability.h>
#include <absl/status/status.h>
#include <absl/status/statusor.h>

namespace symbian::sdl2 {

inline absl::Status SdlError() {
  return absl::InternalError(SDL_GetError());
}

/** Owns an SDL subsystem for a lexical scope. */
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
    if (SDL_InitSubSystem(flags) != 0) {
      return SdlError();
    }
    return Session(flags);
  }

 private:
  explicit Session(std::uint32_t flags) : flags_(flags) {}

  std::uint32_t flags_;
};

/** A unique SDL window. Destroy renderers before destroying their window. */
class Window final {
 public:
  Window(const Window&) = delete;
  Window& operator=(const Window&) = delete;

  Window(Window&& other) noexcept
      : value_(std::exchange(other.value_, nullptr)) {}

  ~Window() {
    if (value_ != nullptr) {
      SDL_DestroyWindow(value_);
    }
  }

  static absl::StatusOr<Window> Create(const char* absl_nonnull title,
                                       int width, int height,
                                       std::uint32_t flags) {
    SDL_Window* absl_nullable value =
        SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED,
                         SDL_WINDOWPOS_UNDEFINED, width, height, flags);
    if (value == nullptr) {
      return SdlError();
    }
    return Window(value);
  }

  static absl::StatusOr<std::unique_ptr<Window>> CreateUnique(
      const char* absl_nonnull title, int width, int height,
      std::uint32_t flags) {
    ABSL_ASSIGN_OR_RETURN(auto created, Create(title, width, height, flags));
    std::unique_ptr<Window> owner(new (std::nothrow)
                                      Window(std::move(created)));
    if (owner == nullptr) {
      return absl::ResourceExhaustedError("SDL window owner allocation failed");
    }
    return owner;
  }

  SDL_Window* absl_nullable get() const { return value_; }

 private:
  explicit Window(SDL_Window* absl_nonnull value) : value_(value) {}

  SDL_Window* absl_nullable value_;
};

/** A unique SDL renderer with status checked drawing operations. */
class Renderer final {
 public:
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  Renderer(Renderer&& other) noexcept
      : value_(std::exchange(other.value_, nullptr)) {}

  Renderer& operator=(Renderer&& other) noexcept {
    if (this != &other) {
      if (value_ != nullptr) {
        SDL_DestroyRenderer(value_);
      }
      value_ = std::exchange(other.value_, nullptr);
    }
    return *this;
  }

  ~Renderer() {
    if (value_ != nullptr) {
      SDL_DestroyRenderer(value_);
    }
  }

  static absl::StatusOr<Renderer> Create(SDL_Window* absl_nonnull window,
                                         std::uint32_t flags) {
    SDL_Renderer* absl_nullable value = SDL_CreateRenderer(window, -1, flags);
    if (value == nullptr) {
      return SdlError();
    }
    return Renderer(value);
  }

  static absl::StatusOr<std::unique_ptr<Renderer>> CreateUnique(
      SDL_Window* absl_nonnull window, std::uint32_t flags) {
    ABSL_ASSIGN_OR_RETURN(auto created, Create(window, flags));
    std::unique_ptr<Renderer> owner(new (std::nothrow)
                                        Renderer(std::move(created)));
    if (owner == nullptr) {
      return absl::ResourceExhaustedError(
          "SDL renderer owner allocation failed");
    }
    return owner;
  }

  SDL_Renderer* absl_nullable get() const { return value_; }

 private:
  explicit Renderer(SDL_Renderer* absl_nonnull value) : value_(value) {}

  SDL_Renderer* absl_nullable value_;
};

}  // namespace symbian::sdl2

#endif  // SYMBIAN_SDL2_SDL2_H_
