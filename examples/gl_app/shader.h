#ifndef SYMBIAN_GL_APP_SHADER_H_
#define SYMBIAN_GL_APP_SHADER_H_

#include <GLES2/gl2.h>

namespace gl_app {
// Returns zero with a guest debug diagnostic on compilation/link failure.
// Attribute locations are fixed: position=0, normal=1.
GLuint CreateProgram(const char* vertex_source, const char* fragment_source);
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_SHADER_H_
