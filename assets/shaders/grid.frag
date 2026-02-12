#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

uniform vec3 uColor;
uniform float uAlpha;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform float uAmbientStrength = 0.14;
uniform float uSpecularStrength = 0.55;
uniform float uShininess = 48.0;

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);

    // Key light (from camera / main direction)
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);

    // Fill light (opposite side, softer) - reduces flat look
    vec3 fillDir = normalize(-uLightPos - FragPos);
    float fillDiff = max(dot(norm, fillDir), 0.0);
    vec3 fillLight = fillDiff * vec3(0.4, 0.45, 0.5);

    // Ambient
    vec3 ambient = uAmbientStrength * vec3(1.0, 1.0, 1.0);

    // Specular (Blinn-Phong - half vector gives nicer highlights)
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), uShininess);
    vec3 specular = uSpecularStrength * spec * vec3(1.0, 1.0, 1.0);

    vec3 result = (ambient + diffuse + fillLight + specular) * uColor;
    // Soft clamp so we don't blow out
    result = result / (1.0 + 0.15 * length(result));
    FragColor = vec4(result, uAlpha);
}
