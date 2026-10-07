// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <new>

#include <absl/base/nullability.h>

#include "symbian/api/display/window_surface.h"
#ifdef SYMBIAN_SDL_GPU
#include "symbian/api/display/gles_window_context.h"
#endif

extern "C" {
#include "SDL.h"
#include "SDL_keyboard_c.h"
#include "SDL_mouse_c.h"
#include "SDL_sysvideo.h"
#include "SDL_windowevents_c.h"
}

namespace {

class VideoOwner final {
 public:
  int Open(SDL_Window* absl_nonnull window) {
    if (window_ != nullptr) {
      return SDL_SetError("Only one window is supported");
    }
    const absl::Status status =
        surface_.Open(window->title == nullptr ? "" : window->title);
    if (!status.ok()) {
      surface_.Close();
      return SDL_SetError("Window open: %s", status.message().data());
    }
    const auto size = surface_.size();
    window->x = 0;
    window->y = 0;
    window->w = size.width;
    window->h = size.height;
    window_ = window;
    return 0;
  }

  int CreateFramebuffer(Uint32* absl_nonnull format,
                        void* absl_nullable* absl_nonnull pixels,
                        int* absl_nonnull pitch) {
    auto frame = surface_.CreateRgb565Frame();
    if (!frame.ok()) {
      return SDL_SetError("Framebuffer: %s", frame.status().message().data());
    }
    *format = SDL_PIXELFORMAT_RGB565;
    *pixels = frame->pixels.data();
    *pitch = frame->pitch_bytes;
    return 0;
  }

  int Present() {
    const absl::Status status = surface_.Present();
    if (!status.ok()) {
      return SDL_SetError("Present: %s", status.message().data());
    }
    return 0;
  }

  void Pump() {
    if (window_ == nullptr) {
      return;
    }
    for (int count = 0; count < 32; ++count) {
      auto input = surface_.PollInput();
      if (!input.ok()) {
        SDL_SetError("Input: %s", input.status().message().data());
        return;
      }
      if (!input->has_value()) {
        return;
      }
      const auto& event = **input;
      switch (event.kind) {
        case symbian::api::display::WindowInputKind::kPointerMove:
        case symbian::api::display::WindowInputKind::kPointerDown:
        case symbian::api::display::WindowInputKind::kPointerUp:
          SDL_SendMouseMotion(window_, 0, 0, event.x, event.y);
          if (event.kind !=
              symbian::api::display::WindowInputKind::kPointerMove) {
            SDL_SendMouseButton(
                window_, 0,
                event.kind ==
                        symbian::api::display::WindowInputKind::kPointerDown
                    ? SDL_PRESSED
                    : SDL_RELEASED,
                SDL_BUTTON_LEFT);
          }
          break;
        case symbian::api::display::WindowInputKind::kKeyDown:
        case symbian::api::display::WindowInputKind::kKeyUp: {
          SDL_Scancode key = SDL_SCANCODE_UNKNOWN;
          if (event.key == symbian::api::display::WindowKey::kEscape ||
              event.key == symbian::api::display::WindowKey::kBackspace) {
            key = SDL_SCANCODE_ESCAPE;
          }
          if (event.key == symbian::api::display::WindowKey::kLeft) {
            key = SDL_SCANCODE_LEFT;
          }
          if (event.key == symbian::api::display::WindowKey::kRight) {
            key = SDL_SCANCODE_RIGHT;
          }
          if (event.key == symbian::api::display::WindowKey::kUp) {
            key = SDL_SCANCODE_UP;
          }
          if (event.key == symbian::api::display::WindowKey::kDown) {
            key = SDL_SCANCODE_DOWN;
          }
          if (event.key == symbian::api::display::WindowKey::kEnter ||
              event.key == symbian::api::display::WindowKey::kSelect) {
            key = SDL_SCANCODE_RETURN;
          }
          if (key != SDL_SCANCODE_UNKNOWN) {
            SDL_SendKeyboardKey(
                event.kind == symbian::api::display::WindowInputKind::kKeyDown
                    ? SDL_PRESSED
                    : SDL_RELEASED,
                key);
          }
        } break;
        case symbian::api::display::WindowInputKind::kFocusGained:
        case symbian::api::display::WindowInputKind::kFocusLost:
          SDL_SendWindowEvent(
              window_,
              event.kind == symbian::api::display::WindowInputKind::kFocusGained
                  ? SDL_WINDOWEVENT_FOCUS_GAINED
                  : SDL_WINDOWEVENT_FOCUS_LOST,
              0, 0);
          break;
        case symbian::api::display::WindowInputKind::kCloseRequested:
          SDL_SendWindowEvent(window_, SDL_WINDOWEVENT_CLOSE, 0, 0);
          break;
        default:
          break;
      }
    }
  }

  void Close() {
#ifdef SYMBIAN_SDL_GPU
    delete gpu_context_;
    gpu_context_ = nullptr;
#endif
    surface_.Close();
    window_ = nullptr;
  }

  void DestroyFramebuffer() { surface_.DestroyFrame(); }

#ifdef SYMBIAN_SDL_GPU
  SDL_GLContext CreateGlContext() {
    if (window_ == nullptr || gpu_context_ != nullptr) {
      SDL_SetError("OpenGL ES window unavailable or context already open");
      return nullptr;
    }
    auto* absl_nullable context =
        new (std::nothrow) symbian::api::display::GlesWindowContext;
    if (context == nullptr) {
      SDL_OutOfMemory();
      return nullptr;
    }
    const absl::Status status = context->Open(&surface_, 2);
    if (!status.ok()) {
      SDL_SetError("OpenGL ES: %s", status.message().data());
      delete context;
      return nullptr;
    }
    gpu_context_ = context;
    return reinterpret_cast<SDL_GLContext>(context);
  }

  int MakeCurrent(SDL_GLContext context) {
    if (context == nullptr && gpu_context_ != nullptr) {
      return GlStatus(gpu_context_->ClearCurrent());
    }
    if (context != reinterpret_cast<SDL_GLContext>(gpu_context_) ||
        gpu_context_ == nullptr) {
      return SDL_SetError("Unknown OpenGL ES context");
    }
    return GlStatus(gpu_context_->MakeCurrent());
  }

  int Swap() {
    return gpu_context_ == nullptr
               ? SDL_SetError("OpenGL ES context unavailable")
               : GlStatus(gpu_context_->Swap());
  }

  int SetSwapInterval(int interval) {
    return gpu_context_ == nullptr
               ? SDL_SetError("OpenGL ES context unavailable")
               : GlStatus(gpu_context_->SetSwapInterval(interval));
  }

  int GetSwapInterval() const {
    return gpu_context_ == nullptr ? 0 : gpu_context_->swap_interval();
  }

  void DeleteContext(SDL_GLContext context) {
    if (context == reinterpret_cast<SDL_GLContext>(gpu_context_)) {
      delete gpu_context_;
      gpu_context_ = nullptr;
    }
  }

  void DrawableSize(int* absl_nonnull width, int* absl_nonnull height) const {
    const auto size = surface_.size();
    *width = size.width;
    *height = size.height;
  }
#endif

 private:
#ifdef SYMBIAN_SDL_GPU
  static int GlStatus(const absl::Status& status) {
    return status.ok() ? 0
                       : SDL_SetError("OpenGL ES: %s", status.message().data());
  }

  symbian::api::display::GlesWindowContext* absl_nullable gpu_context_ =
      nullptr;
#endif
  symbian::api::display::WindowSurface surface_;
  SDL_Window* absl_nullable window_ = nullptr;
};

VideoOwner* absl_nonnull Owner(SDL_VideoDevice* absl_nonnull device) {
  return static_cast<VideoOwner*>(device->driverdata);
}

int VideoInit(SDL_VideoDevice* absl_nonnull device) {
  SDL_DisplayMode mode = {};
  mode.format = SDL_PIXELFORMAT_RGB565;
  auto size = symbian::api::display::WindowSurface::PrimarySize();
  if (!size.ok()) {
    return SDL_SetError("Display: %s", size.status().message().data());
  }
  mode.w = size->width;
  mode.h = size->height;
  mode.refresh_rate = static_cast<int>(
      symbian::api::display::WindowSurface::PrimaryRefreshRateHz().value_or(0));
  return SDL_AddBasicVideoDisplay(&mode);
}

void VideoQuit(SDL_VideoDevice* absl_nonnull device) {
  Owner(device)->Close();
}

int CreateWindow(SDL_VideoDevice* absl_nonnull device,
                 SDL_Window* absl_nonnull window) {
  return Owner(device)->Open(window);
}

void DestroyWindow(SDL_VideoDevice* absl_nonnull device,
                   SDL_Window* absl_nonnull window) {
  Owner(device)->Close();
}

int CreateFramebuffer(SDL_VideoDevice* absl_nonnull device,
                      SDL_Window* absl_nonnull window,
                      Uint32* absl_nonnull format,
                      void* absl_nullable* absl_nonnull pixels,
                      int* absl_nonnull pitch) {
  return Owner(device)->CreateFramebuffer(format, pixels, pitch);
}

int UpdateFramebuffer(SDL_VideoDevice* absl_nonnull device,
                      SDL_Window* absl_nonnull window,
                      const SDL_Rect* absl_nullable rects, int numrects) {
  return Owner(device)->Present();
}

void DestroyFramebuffer(SDL_VideoDevice* absl_nonnull device,
                        SDL_Window* absl_nonnull window) {
  Owner(device)->DestroyFramebuffer();
}

void PumpEvents(SDL_VideoDevice* absl_nonnull device) {
  Owner(device)->Pump();
}

#ifdef SYMBIAN_SDL_GPU
int GlLoadLibrary(SDL_VideoDevice* absl_nonnull device,
                  const char* absl_nullable path) {
  return 0;
}

void* absl_nullable GlGetProcAddress(SDL_VideoDevice* absl_nonnull device,
                                     const char* absl_nonnull name) {
  return reinterpret_cast<void*>(
      symbian::api::display::GlesWindowContext::GetProcAddress(name));
}

void GlUnloadLibrary(SDL_VideoDevice* absl_nonnull device) {}

SDL_GLContext GlCreateContext(SDL_VideoDevice* absl_nonnull device,
                              SDL_Window* absl_nonnull window) {
  return Owner(device)->CreateGlContext();
}

int GlMakeCurrent(SDL_VideoDevice* absl_nonnull device,
                  SDL_Window* absl_nullable window, SDL_GLContext context) {
  return Owner(device)->MakeCurrent(context);
}

void GlDrawableSize(SDL_VideoDevice* absl_nonnull device,
                    SDL_Window* absl_nonnull window, int* absl_nonnull width,
                    int* absl_nonnull height) {
  Owner(device)->DrawableSize(width, height);
}

int GlSetSwapInterval(SDL_VideoDevice* absl_nonnull device, int interval) {
  return Owner(device)->SetSwapInterval(interval);
}

int GlGetSwapInterval(SDL_VideoDevice* absl_nonnull device) {
  return Owner(device)->GetSwapInterval();
}

int GlSwapWindow(SDL_VideoDevice* absl_nonnull device,
                 SDL_Window* absl_nonnull window) {
  return Owner(device)->Swap();
}

void GlDeleteContext(SDL_VideoDevice* absl_nonnull device,
                     SDL_GLContext context) {
  Owner(device)->DeleteContext(context);
}

void GlDefaultProfile(SDL_VideoDevice* absl_nonnull device,
                      int* absl_nonnull mask, int* absl_nonnull major,
                      int* absl_nonnull minor) {
  *mask = SDL_GL_CONTEXT_PROFILE_ES;
  *major = 2;
  *minor = 0;
}
#endif

void DeleteDevice(SDL_VideoDevice* absl_nonnull device) {
  delete Owner(device);
  SDL_free(device);
}

SDL_VideoDevice* absl_nullable CreateDevice() {
  auto* absl_nullable device =
      static_cast<SDL_VideoDevice*>(SDL_calloc(1, sizeof(SDL_VideoDevice)));
  if (device == nullptr) {
    return nullptr;
  }
  auto* absl_nullable owner = new (std::nothrow) VideoOwner;
  if (owner == nullptr) {
    SDL_free(device);
    SDL_OutOfMemory();
    return nullptr;
  }
  device->driverdata = owner;
  device->VideoInit = VideoInit;
  device->VideoQuit = VideoQuit;
  device->CreateSDLWindow = CreateWindow;
  device->DestroyWindow = DestroyWindow;
  device->CreateWindowFramebuffer = CreateFramebuffer;
  device->UpdateWindowFramebuffer = UpdateFramebuffer;
  device->DestroyWindowFramebuffer = DestroyFramebuffer;
  device->PumpEvents = PumpEvents;
#ifdef SYMBIAN_SDL_GPU
  device->GL_LoadLibrary = GlLoadLibrary;
  device->GL_GetProcAddress = GlGetProcAddress;
  device->GL_UnloadLibrary = GlUnloadLibrary;
  device->GL_CreateContext = GlCreateContext;
  device->GL_MakeCurrent = GlMakeCurrent;
  device->GL_GetDrawableSize = GlDrawableSize;
  device->GL_SetSwapInterval = GlSetSwapInterval;
  device->GL_GetSwapInterval = GlGetSwapInterval;
  device->GL_SwapWindow = GlSwapWindow;
  device->GL_DeleteContext = GlDeleteContext;
  device->GL_DefaultProfileConfig = GlDefaultProfile;
#endif
  device->free = DeleteDevice;
  return device;
}

}  // namespace

extern "C" VideoBootStrap SYMBIAN_bootstrap = {
    "symbian", "Symbian Window Server", CreateDevice, nullptr};
