#include "cube.h"

#include <cstddef>

#include <math.h>

#include "shader.h"
#include "shaders.h"

namespace gl_app {

bool Cube::Open() {
  program_ = CreateProgram(shaders::kCubeVertex, shaders::kCubeFragment);
  if (!program_) {
    return false;
  }
  model_view_ = glGetUniformLocation(program_, "uModelView");
  projection_ = glGetUniformLocation(program_, "uProjection");
  MakeGeometry();
  glGenBuffers(1, &vertex_buffer_);
  if (vertex_buffer_ != 0) {
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices_), vertices_, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    if (glGetError() != GL_NO_ERROR) {
      glDeleteBuffers(1, &vertex_buffer_);
      vertex_buffer_ = 0;
    }
  }
  return true;
}

void Cube::Close() {
  if (vertex_buffer_ != 0) {
    glDeleteBuffers(1, &vertex_buffer_);
    vertex_buffer_ = 0;
  }
  if (program_) {
    glDeleteProgram(program_);
  }
  program_ = 0;
}

void Cube::Draw(TSize size, float yaw, float pitch) {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glUseProgram(program_);
  const float c = cosf(yaw), s = sinf(yaw);
  const float cx = cosf(pitch), sx = sinf(pitch);
  const GLfloat model_view[] = {c, sx * s,  -cx * s, 0, 0, cx,    sx,    0,
                                s, -sx * c, cx * c,  0, 0, 0.25f, -7.2f, 1};
  const float aspect = static_cast<float>(size.iWidth) / size.iHeight;
  const GLfloat projection[] = {
      2.0f / aspect, 0,  0, 0, 0,           2.0f, 0, 0, 0, 0,
      -1.020202f,    -1, 0, 0, -0.2020202f, 0};
  glUniformMatrix4fv(model_view_, 1, GL_FALSE, model_view);
  glUniformMatrix4fv(projection_, 1, GL_FALSE, projection);
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glEnableVertexAttribArray(2);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        vertex_buffer_ != 0 ? reinterpret_cast<const void*>(
                                                  offsetof(Vertex, position))
                                            : vertices_[0].position);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        vertex_buffer_ != 0 ? reinterpret_cast<const void*>(
                                                  offsetof(Vertex, normal))
                                            : vertices_[0].normal);
  glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        vertex_buffer_ != 0 ? reinterpret_cast<const void*>(
                                                  offsetof(Vertex, color))
                                            : vertices_[0].color);
  glDrawArrays(GL_TRIANGLES, 0, 36);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(2);
  glDisableVertexAttribArray(1);
  glDisableVertexAttribArray(0);
}

void Cube::MakeGeometry() {
  constexpr GLfloat corners[8][3] = {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1},
                                     {-1, 1, -1},  {-1, -1, 1}, {1, -1, 1},
                                     {1, 1, 1},    {-1, 1, 1}};
  constexpr int faces[6][4] = {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6},
                               {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}};
  constexpr GLfloat normals[6][3] = {{0, 0, 1},  {0, 0, -1}, {1, 0, 0},
                                     {-1, 0, 0}, {0, 1, 0},  {0, -1, 0}};
  constexpr GLfloat colors[6][3] = {
      {0.84f, 0.09f, 0.09f}, {0.72f, 0.06f, 0.06f}, {0.08f, 0.68f, 0.13f},
      {0.05f, 0.56f, 0.10f}, {0.09f, 0.25f, 0.92f}, {0.07f, 0.19f, 0.78f}};
  constexpr int triangle[6] = {0, 1, 2, 0, 2, 3};
  for (int face = 0; face < 6; ++face) {
    for (int point = 0; point < 6; ++point) {
      for (int axis = 0; axis < 3; ++axis) {
        vertices_[face * 6 + point].position[axis] =
            corners[faces[face][triangle[point]]][axis];
        vertices_[face * 6 + point].normal[axis] = normals[face][axis];
        vertices_[face * 6 + point].color[axis] = colors[face][axis];
      }
    }
  }
}

}  // namespace gl_app
