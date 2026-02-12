#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
void main() {
    vec3 color = mix(uBottomColor, uTopColor, TexCoords.y);
    FragColor = vec4(color, 1.0);
}
