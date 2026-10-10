#include "renderer.h"

#include <absl/base/nullability.h>

namespace gl_app {
Renderer::~Renderer() {
  if (objects_open_) {
    cube_.Close();
    ui_batch_.Close();
  }
}

absl::Status Renderer::Open(
    symbian::api::display::WindowSurface* absl_nonnull window) {
  if (const absl::Status opened = context_.Open(
          window, 2,
          {.red_bits = 8, .green_bits = 8, .blue_bits = 8, .depth_bits = 16});
      !opened.ok()) {
    return opened;
  }
  if (!cube_.Open() || !ui_batch_.Open().ok()) {
    cube_.Close();
    ui_batch_.Close();
    return absl::UnavailableError("cube shader setup failed");
  }
  objects_open_ = true;
  context_.SetSwapInterval(1).IgnoreError();
  return absl::OkStatus();
}

absl::Status Renderer::Draw(float yaw, float pitch, bool paused,
                            std::uint32_t frames_per_second) {
  glViewport(0, 0, size_.iWidth, size_.iHeight);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  cube_.Draw(size_, yaw, pitch);
  ui_batch_.Begin(size_.iWidth, size_.iHeight);
  if (paused) {
    pause_panel_.Draw(size_, &ui_batch_);
  } else {
    exit_button_.Draw(size_, &ui_batch_);
  }
  pause_panel_.DrawFrameRate(size_, frames_per_second, &ui_batch_);
  ui_batch_.Draw();
  return context_.Swap();
}

}  // namespace gl_app
