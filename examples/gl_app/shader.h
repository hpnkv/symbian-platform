#ifndef SYMBIAN_GL_APP_SHADER_H_
#define SYMBIAN_GL_APP_SHADER_H_

#include <string_view>

#include <GLES2/gl2.h>
#include <absl/base/nullability.h>

namespace gl_app {
// Returns zero with a guest debug diagnostic on compilation/link failure.
// Attribute locations are fixed: position=0, normal=1.
GLuint CreateProgram(std::string_view vertex_source,
                     std::string_view fragment_source);
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_SHADER_H_
