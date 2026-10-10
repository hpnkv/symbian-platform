#ifndef SYMBIAN_GL_APP_APPLICATION_H_
#define SYMBIAN_GL_APP_APPLICATION_H_

#include "absl/status/status.h"

namespace gl_app {
// Owns the Window Server session, native windows, frame timer and input loop.
absl::Status RunApplication();
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_APPLICATION_H_
