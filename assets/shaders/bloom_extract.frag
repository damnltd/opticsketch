#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D uScene;
uniform float uThreshold = 0.8;
void main() {
    vec3 color = texture(uScene, TexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > uThreshold) {
        FragColor = vec4(color * (brightness - uThreshold), 1.0);
    } else {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
    }
}
