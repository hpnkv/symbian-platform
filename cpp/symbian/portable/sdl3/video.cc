// Copyright 2026 The Symbian SDK Authors.
// Licensed under the Apache License, Version 2.0.

#include <cstdint>
#include <new>

#include <absl/base/nullability.h>

#include "symbian/api/display/window_surface.h"
#ifdef SYMBIAN_SDL_GPU
#include "symbian/api/display/gles_window_context.h"
#endif

extern "C" {
#include "SDL_internal.h"
#include "SDL_keyboard_c.h"
#include "SDL_mouse_c.h"
#include "SDL_sysvideo.h"
#include "SDL_windowevents_c.h"
}

namespace {

class VideoOwner final {
 public:
  bool Open(SDL_Window* absl_nonnull window) {
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
    window->windowed.w = size.width;
    window->windowed.h = size.height;
    window->floating.w = size.width;
    window->floating.h = size.height;
    window->pending.w = size.width;
    window->pending.h = size.height;
    window_ = window;
    return true;
  }

  bool CreateFramebuffer(SDL_PixelFormat* absl_nonnull format,
                         void* absl_nullable* absl_nonnull pixels,
                         int* absl_nonnull pitch) {
    auto frame = surface_.CreateRgb565Frame();
    if (!frame.ok()) {
      return SDL_SetError("Framebuffer: %s", frame.status().message().data());
    }
    *format = SDL_PIXELFORMAT_RGB565;
    *pixels = frame->pixels.data();
    *pitch = frame->pitch_bytes;
    return true;
  }

  bool Present() {
    const absl::Status status = surface_.Present();
    if (!status.ok()) {
      return SDL_SetError("Present: %s", status.message().data());
    }
    return true;
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
          SDL_SendMouseMotion(0, window_, 0, false, static_cast<float>(event.x),
                              static_cast<float>(event.y));
          if (event.kind !=
              symbian::api::display::WindowInputKind::kPointerMove) {
            SDL_SendMouseButton(
                0, window_, 0, SDL_BUTTON_LEFT,
                event.kind ==
                    symbian::api::display::WindowInputKind::kPointerDown);
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
                0, 0, 0, key,
                event.kind == symbian::api::display::WindowInputKind::kKeyDown);
          }
          break;
        }
        case symbian::api::display::WindowInputKind::kFocusGained:
        case symbian::api::display::WindowInputKind::kFocusLost:
          SDL_SendWindowEvent(
              window_,
              event.kind == symbian::api::display::WindowInputKind::kFocusGained
                  ? SDL_EVENT_WINDOW_FOCUS_GAINED
                  : SDL_EVENT_WINDOW_FOCUS_LOST,
              0, 0);
          break;
        case symbian::api::display::WindowInputKind::kCloseRequested:
          SDL_SendWindowEvent(window_, SDL_EVENT_WINDOW_CLOSE_REQUESTED, 0, 0);
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

  bool MakeCurrent(SDL_GLContext context) {
    if (context == nullptr && gpu_context_ != nullptr) {
      return GlStatus(gpu_context_->ClearCurrent());
    }
    if (context != reinterpret_cast<SDL_GLContext>(gpu_context_) ||
        gpu_context_ == nullptr) {
      return SDL_SetError("Unknown OpenGL ES context");
    }
    return GlStatus(gpu_context_->MakeCurrent());
  }

  bool Swap() {
    return gpu_context_ == nullptr
               ? SDL_SetError("OpenGL ES context unavailable")
               : GlStatus(gpu_context_->Swap());
  }

  bool SetSwapInterval(int interval) {
    return gpu_context_ == nullptr
               ? SDL_SetError("OpenGL ES context unavailable")
               : GlStatus(gpu_context_->SetSwapInterval(interval));
  }

  bool GetSwapInterval(int* absl_nonnull interval) const {
    if (gpu_context_ == nullptr) {
      return SDL_SetError("OpenGL ES context unavailable");
    }
    *interval = gpu_context_->swap_interval();
    return true;
  }

  bool DeleteContext(SDL_GLContext context) {
    if (context != reinterpret_cast<SDL_GLContext>(gpu_context_)) {
      return SDL_SetError("Unknown OpenGL ES context");
    }
    delete gpu_context_;
    gpu_context_ = nullptr;
    return true;
  }
#endif

 private:
#ifdef SYMBIAN_SDL_GPU
  static bool GlStatus(const absl::Status& status) {
    return status.ok() ? true
                       : SDL_SetError("OpenGL ES: %s", status.message().data());
  }

  symbian::api::display::GlesWindowContext* absl_nullable gpu_context_ =
      nullptr;
#endif
  symbian::api::display::WindowSurface surface_;
  SDL_Window* absl_nullable window_ = nullptr;
};

VideoOwner* absl_nonnull Owner(SDL_VideoDevice* absl_nonnull device) {
  return reinterpret_cast<VideoOwner*>(device->internal);
}

bool VideoInit(SDL_VideoDevice* absl_nonnull device) {
#ifndef SYMBIAN_SDL_GPU
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "software");
#endif
  SDL_DisplayMode mode = {};
  mode.format = SDL_PIXELFORMAT_RGB565;
  auto size = symbian::api::display::WindowSurface::PrimarySize();
  if (!size.ok()) {
    return SDL_SetError("Display: %s", size.status().message().data());
  }
  mode.w = size->width;
  mode.h = size->height;
  const std::uint32_t refresh_rate =
      symbian::api::display::WindowSurface::PrimaryRefreshRateHz().value_or(0);
  mode.refresh_rate = static_cast<float>(refresh_rate);
  mode.refresh_rate_numerator = static_cast<int>(refresh_rate);
  mode.refresh_rate_denominator = refresh_rate == 0 ? 0 : 1;
  return SDL_AddBasicVideoDisplay(&mode) != 0;
}

void VideoQuit(SDL_VideoDevice* absl_nonnull device) {
  Owner(device)->Close();
}

bool CreateWindow(SDL_VideoDevice* absl_nonnull device,
                  SDL_Window* absl_nonnull window, SDL_PropertiesID props) {
  return Owner(device)->Open(window);
}

void DestroyWindow(SDL_VideoDevice* absl_nonnull device,
                   SDL_Window* absl_nonnull window) {
  Owner(device)->Close();
}

bool CreateFramebuffer(SDL_VideoDevice* absl_nonnull device,
                       SDL_Window* absl_nonnull window,
                       SDL_PixelFormat* absl_nonnull format,
                       void* absl_nullable* absl_nonnull pixels,
                       int* absl_nonnull pitch) {
  return Owner(device)->CreateFramebuffer(format, pixels, pitch);
}

bool UpdateFramebuffer(SDL_VideoDevice* absl_nonnull device,
                       SDL_Window* absl_nonnull window,
                       const SDL_Rect* absl_nullable rects, int count) {
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
bool GlLoadLibrary(SDL_VideoDevice* absl_nonnull device,
                   const char* absl_nullable path) {
  return true;
}

SDL_FunctionPointer absl_nullable GlGetProcAddress(
    SDL_VideoDevice* absl_nonnull device, const char* absl_nonnull name) {
  return reinterpret_cast<SDL_FunctionPointer>(
      symbian::api::display::GlesWindowContext::GetProcAddress(name));
}

void GlUnloadLibrary(SDL_VideoDevice* absl_nonnull device) {}

SDL_GLContext GlCreateContext(SDL_VideoDevice* absl_nonnull device,
                              SDL_Window* absl_nonnull window) {
  return Owner(device)->CreateGlContext();
}

bool GlMakeCurrent(SDL_VideoDevice* absl_nonnull device,
                   SDL_Window* absl_nullable window, SDL_GLContext context) {
  return Owner(device)->MakeCurrent(context);
}

bool GlSetSwapInterval(SDL_VideoDevice* absl_nonnull device, int interval) {
  return Owner(device)->SetSwapInterval(interval);
}

bool GlGetSwapInterval(SDL_VideoDevice* absl_nonnull device,
                       int* absl_nonnull interval) {
  return Owner(device)->GetSwapInterval(interval);
}

bool GlSwapWindow(SDL_VideoDevice* absl_nonnull device,
                  SDL_Window* absl_nonnull window) {
  return Owner(device)->Swap();
}

bool GlDeleteContext(SDL_VideoDevice* absl_nonnull device,
                     SDL_GLContext context) {
  return Owner(device)->DeleteContext(context);
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
  device->internal = reinterpret_cast<SDL_VideoData*>(owner);
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
  device->GL_SetSwapInterval = GlSetSwapInterval;
  device->GL_GetSwapInterval = GlGetSwapInterval;
  device->GL_SwapWindow = GlSwapWindow;
  device->GL_DestroyContext = GlDeleteContext;
  device->GL_DefaultProfileConfig = GlDefaultProfile;
#endif
  device->free = DeleteDevice;
  return device;
}

}  // namespace

extern "C" VideoBootStrap SYMBIAN_bootstrap = {
    "symbian", "Symbian Window Server", CreateDevice, nullptr, false};
