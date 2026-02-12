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

// Material uniforms
uniform float uMetallic = 0.0;
uniform float uRoughness = 0.5;
uniform float uTransparency = 0.0;
uniform float uFresnelIOR = 1.5;

// HDRI environment map (equirectangular)
uniform sampler2D uEnvMap;
uniform bool uHasEnvMap = false;
uniform float uEnvIntensity = 1.0;
uniform float uEnvRotation = 0.0; // radians

const float PI = 3.14159265359;

vec3 sampleEquirectangular(vec3 dir, float rotation) {
    // Rotate direction around Y axis
    float cosR = cos(rotation);
    float sinR = sin(rotation);
    vec3 rd = vec3(cosR * dir.x + sinR * dir.z, dir.y, -sinR * dir.x + cosR * dir.z);
    // Convert direction to equirectangular UV
    float u = atan(rd.z, rd.x) / (2.0 * PI) + 0.5;
    float v = asin(clamp(rd.y, -1.0, 1.0)) / PI + 0.5;
    return texture(uEnvMap, vec2(u, v)).rgb;
}

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 lightDir = normalize(uLightPos - FragPos);

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);

    // Fill light (opposite side)
    vec3 fillDir = normalize(-uLightPos - FragPos);
    float fillDiff = max(dot(norm, fillDir), 0.0);
    vec3 fillLight = fillDiff * vec3(0.4, 0.45, 0.5);

    // Ambient
    vec3 ambient = uAmbientStrength * vec3(1.0);

    // Specular (Blinn-Phong with roughness-adjusted shininess)
    float shininess = max(2.0, uShininess * (1.0 - uRoughness * 0.9));
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);

    // Fresnel (Schlick approximation)
    float cosTheta = max(dot(viewDir, norm), 0.0);
    float f0 = (uFresnelIOR - 1.0) / (uFresnelIOR + 1.0);
    f0 = f0 * f0;
    // Metallic surfaces use color as f0
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);

    // Metallic tinting: metals reflect their base color in specular
    vec3 specColor = mix(vec3(1.0), uColor, uMetallic);
    float specStrength = mix(uSpecularStrength, uSpecularStrength * 2.0, uMetallic);
    vec3 specular = specStrength * spec * specColor;

    // Metallic reduces diffuse (metals absorb rather than diffuse light)
    vec3 baseColor = uColor * (1.0 - uMetallic * 0.7);

    vec3 result = (ambient + diffuse + fillLight) * baseColor + specular;

    // HDRI environment reflections
    if (uHasEnvMap) {
        vec3 reflectDir = reflect(-viewDir, norm);
        vec3 envColor = sampleEquirectangular(reflectDir, uEnvRotation);
        // Roughness attenuates reflection sharpness (approximation without prefiltered mips)
        float envStrength = 1.0 - uRoughness * 0.85;
        // Metallic surfaces reflect their base color; dielectrics reflect white
        vec3 envTint = mix(vec3(1.0), uColor, uMetallic);
        result += envColor * envTint * envStrength * fresnel * uEnvIntensity;
    }

    // Fresnel rim for glass/transparent materials
    if (uTransparency > 0.01) {
        result += fresnel * vec3(0.3) * uTransparency;
    }

    // Tone mapping
    result = result / (1.0 + 0.15 * length(result));

    // Alpha: transparency reduces alpha, Fresnel increases it at edges
    float alpha = uAlpha * (1.0 - uTransparency * (1.0 - fresnel * 0.5));

    FragColor = vec4(result, alpha);
}
