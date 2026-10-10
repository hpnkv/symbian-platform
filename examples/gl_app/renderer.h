#ifndef SYMBIAN_GL_APP_RENDERER_H_
#define SYMBIAN_GL_APP_RENDERER_H_

#include <cstdint>
#include <memory>

#include <absl/status/status.h>
#include <absl/status/statusor.h>
#include <w32std.h>

#include "pause_panel.h"
#include "symbian/api/display/window_surface.h"

namespace gl_app {
// Destroys GL objects before the SDK-owned context and window are closed.
class Renderer {
 public:
  static absl::StatusOr<Renderer> Create(
      symbian::api::display::WindowSurface* absl_nonnull window);
  static absl::StatusOr<std::unique_ptr<Renderer>> CreateUnique(
      symbian::api::display::WindowSurface* absl_nonnull window);
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) noexcept;

  void Resize(TSize size);

  bool HandlePointer(const symbian::api::display::WindowInput& pointer);

  PausePanel::Action HandlePausePointer(
      const symbian::api::display::WindowInput& pointer);

  absl::Status Draw(float yaw, float pitch, bool paused,
                    std::uint32_t frames_per_second);

 private:
  struct Impl;
  explicit Renderer(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_RENDERER_H_
