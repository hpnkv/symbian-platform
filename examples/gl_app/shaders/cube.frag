// Eye-space Phong lighting: diffuse point light and directional light.
precision mediump float;
varying vec3 vPosition;
varying vec3 vNormal;
varying vec3 vColor;
void main() {
  vec3 normal = normalize(vNormal);
  vec3 view = normalize(-vPosition);
  vec3 pointLight = normalize(vec3(3.0, 2.5, 0.0) - vPosition);
  vec3 directedLight = normalize(vec3(-0.7, 0.5, 1.0));
  float pointDiffuse = max(dot(normal, pointLight), 0.0);
  float directedDiffuse = max(dot(normal, directedLight), 0.0);
  float pointSpecular = pointDiffuse > 0.0 ?
    pow(max(dot(reflect(-pointLight, normal), view), 0.0), 32.0) : 0.0;
  float directedSpecular = directedDiffuse > 0.0 ?
    pow(max(dot(reflect(-directedLight, normal), view), 0.0), 32.0) : 0.0;
  vec3 material = vColor;
  vec3 color = material * (vec3(0.09) +
    vec3(1.0, 0.77, 0.48) * pointDiffuse +
    vec3(0.37, 0.62, 1.0) * directedDiffuse) +
    vec3(1.0, 0.84, 0.64) * pointSpecular * 0.6 +
    vec3(0.60, 0.78, 1.0) * directedSpecular * 0.7;
  gl_FragColor = vec4(color, 1.0);
}
