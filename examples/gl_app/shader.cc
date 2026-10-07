#include "shader.h"

#include <absl/base/nullability.h>
#include <e32debug.h>

namespace gl_app {
namespace {

void ShaderDiagnostic(GLuint object, bool program) {
  char log[240] = {};
  if (program) {
    glGetProgramInfoLog(object, sizeof(log), nullptr, log);
  } else {
    glGetShaderInfoLog(object, sizeof(log), nullptr, log);
  }
  TBuf<240> message;
  for (int i = 0; log[i] && i < 239; ++i) {
    message.Append(static_cast<TUint8>(log[i]));
  }
  RDebug::Print(_L("gl_app shader: %S"), &message);
}

GLuint CompileShader(GLenum type, std::string_view source) {
  GLuint shader = glCreateShader(type);
  if (!shader) {
    return 0;
  }
  const char* absl_nonnull data = source.empty() ? "" : source.data();
  const GLint length = static_cast<GLint>(source.size());
  glShaderSource(shader, 1, &data, &length);
  glCompileShader(shader);
  GLint compiled = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    ShaderDiagnostic(shader, false);
    glDeleteShader(shader);
    return 0;
  }
  return shader;
}

}  // namespace

GLuint CreateProgram(std::string_view vertex_source,
                     std::string_view fragment_source) {
  GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertex_source);
  GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragment_source);
  GLuint program = 0;
  if (vertex && fragment) {
    program = glCreateProgram();
    if (program) {
      glAttachShader(program, vertex);
      glAttachShader(program, fragment);
      glBindAttribLocation(program, 0, "aPosition");
      glBindAttribLocation(program, 1, "aNormal");
      glBindAttribLocation(program, 2, "aColor");
      glLinkProgram(program);
      GLint linked = 0;
      glGetProgramiv(program, GL_LINK_STATUS, &linked);
      if (!linked) {
        ShaderDiagnostic(program, true);
        glDeleteProgram(program);
        program = 0;
      }
    }
  }
  if (vertex) {
    glDeleteShader(vertex);
  }
  if (fragment) {
    glDeleteShader(fragment);
  }
  return program;
}

}  // namespace gl_app
