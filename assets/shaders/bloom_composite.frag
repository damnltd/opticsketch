#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity = 1.0;
void main() {
    vec3 sceneColor = texture(uScene, TexCoords).rgb;
    vec3 bloomColor = texture(uBloom, TexCoords).rgb;
    vec3 result = sceneColor + bloomColor * uBloomIntensity;
    // Reinhard tone mapping
    result = result / (1.0 + result);
    FragColor = vec4(result, 1.0);
}
