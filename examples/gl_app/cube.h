#ifndef SYMBIAN_GL_APP_CUBE_H_
#define SYMBIAN_GL_APP_CUBE_H_

#include <GLES2/gl2.h>
#include <gdi.h>

namespace gl_app {
class Cube {
 public:
  Cube() = default;
  Cube(const Cube&) = delete;
  Cube& operator=(const Cube&) = delete;
  // Open, draw and close on the thread with the owning EGL context current.
  bool Open();
  void Draw(TSize size, float yaw, float pitch);
  void Close();

 private:
  struct Vertex {
    GLfloat position[3];
    GLfloat normal[3];
    GLfloat color[3];
  };

  void MakeGeometry();
  GLuint program_ = 0;
  GLuint vertex_buffer_ = 0;
  GLint model_view_ = -1;
  GLint projection_ = -1;
  Vertex vertices_[36];
};
}  // namespace gl_app

#endif  // SYMBIAN_GL_APP_CUBE_H_
