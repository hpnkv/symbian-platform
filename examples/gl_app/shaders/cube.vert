attribute vec3 aPosition;
attribute vec3 aNormal;
attribute vec3 aColor;
uniform mat4 uModelView;
uniform mat4 uProjection;
varying vec3 vPosition;
varying vec3 vNormal;
varying vec3 vColor;
void main() {
  vec4 position = uModelView * vec4(aPosition, 1.0);
  vPosition = position.xyz;
  vNormal = mat3(uModelView) * aNormal;
  vColor = aColor;
  gl_Position = uProjection * position;
}
