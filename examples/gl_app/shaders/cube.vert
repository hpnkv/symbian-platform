attribute vec3 aPosition;
attribute vec3 aNormal;
uniform mat4 uModelView;
uniform mat4 uProjection;
varying vec3 vPosition;
varying vec3 vNormal;
void main() {
  vec4 position = uModelView * vec4(aPosition, 1.0);
  vPosition = position.xyz;
  vNormal = mat3(uModelView) * aNormal;
  gl_Position = uProjection * position;
}
