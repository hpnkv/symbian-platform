#include "renderer.h"

#include <memory>
#include <new>
#include <optional>
#include <utility>

#include <absl/base/nullability.h>
#include <absl/status/status_macros.h>

#include "cube.h"
#include "exit_button.h"
#include "symbian/api/display/gles_rect_batch.h"
#include "symbian/api/display/gles_window_context.h"

namespace gl_app {

struct Renderer::Impl {
  std::optional<symbian::api::display::GlesWindowContext> context;
  std::optional<Cube> cube;
  std::optional<symbian::api::display::GlesRectBatch> ui_batch;
  ExitButton exit_button;
  PausePanel pause_panel;
  TSize size{0, 0};

  ~Impl() {
    cube.reset();
    if (ui_batch.has_value()) {
      ui_batch->Close();
    }
  }
};

Renderer::Renderer(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

Renderer::Renderer(Renderer&& other) noexcept = default;
Renderer& Renderer::operator=(Renderer&& other) noexcept = default;
Renderer::~Renderer() = default;

absl::StatusOr<Renderer> Renderer::Create(
    symbian::api::display::WindowSurface* absl_nonnull window) {
  std::unique_ptr<Impl> impl(new (std::nothrow) Impl);
  if (impl == nullptr) {
    return absl::ResourceExhaustedError("GL renderer owner allocation failed");
  }
  ABSL_ASSIGN_OR_RETURN(
      auto context,
      symbian::api::display::GlesWindowContext::Create(
          window, 2,
          {.red_bits = 8, .green_bits = 8, .blue_bits = 8, .depth_bits = 16}));
  impl->context = std::move(context);
  ABSL_ASSIGN_OR_RETURN(auto cube, Cube::Create());
  impl->cube = std::move(cube);
  ABSL_ASSIGN_OR_RETURN(auto ui_batch,
                        symbian::api::display::GlesRectBatch::Create());
  impl->ui_batch = std::move(ui_batch);
  impl->context->SetSwapInterval(1).IgnoreError();
  return Renderer(std::move(impl));
}

absl::StatusOr<std::unique_ptr<Renderer>> Renderer::CreateUnique(
    symbian::api::display::WindowSurface* absl_nonnull window) {
  ABSL_ASSIGN_OR_RETURN(auto created, Create(window));
  std::unique_ptr<Renderer> owner(new (std::nothrow)
                                      Renderer(std::move(created)));
  if (owner == nullptr) {
    return absl::ResourceExhaustedError("GL renderer owner allocation failed");
  }
  return owner;
}

void Renderer::Resize(TSize size) {
  if (impl_ != nullptr) {
    impl_->size = size;
  }
}

bool Renderer::HandlePointer(
    const symbian::api::display::WindowInput& pointer) {
  return impl_ != nullptr &&
         impl_->exit_button.HandlePointer(pointer, impl_->size);
}

PausePanel::Action Renderer::HandlePausePointer(
    const symbian::api::display::WindowInput& pointer) {
  return impl_ == nullptr
             ? PausePanel::Action::kNone
             : impl_->pause_panel.HandlePointer(pointer, impl_->size);
}

absl::Status Renderer::Draw(float yaw, float pitch, bool paused,
                            std::uint32_t frames_per_second) {
  if (impl_ == nullptr) {
    return absl::FailedPreconditionError("GL renderer was moved");
  }
  glViewport(0, 0, impl_->size.iWidth, impl_->size.iHeight);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  impl_->cube->Draw(impl_->size, yaw, pitch);
  impl_->ui_batch->Begin(impl_->size.iWidth, impl_->size.iHeight);
  if (paused) {
    impl_->pause_panel.Draw(impl_->size, &*impl_->ui_batch);
  } else {
    impl_->exit_button.Draw(impl_->size, &*impl_->ui_batch);
  }
  impl_->pause_panel.DrawFrameRate(impl_->size, frames_per_second,
                                   &*impl_->ui_batch);
  impl_->ui_batch->Draw();
  return impl_->context->Swap();
}

}  // namespace gl_app
