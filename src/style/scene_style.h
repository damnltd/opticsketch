#pragma once

#include <glm/glm.hpp>
#include <array>
#include <string>

namespace opticsketch {

static constexpr int kElementTypeCount = 14;

enum class RenderMode { Standard, Schematic, Presentation };
enum class BackgroundMode { Solid, Gradient };

struct SceneStyle {
    // Render mode
    RenderMode renderMode = RenderMode::Standard;
    // Element colors (indexed by (int)ElementType)
    std::array<glm::vec3, kElementTypeCount> elementColors;

    // Viewport
    BackgroundMode bgMode = BackgroundMode::Solid;
    glm::vec3 bgColor{0.05f, 0.05f, 0.06f};
    glm::vec3 bgGradientTop{0.12f, 0.12f, 0.15f};
    glm::vec3 bgGradientBottom{0.02f, 0.02f, 0.03f};
    glm::vec3 gridColor{0.3f, 0.3f, 0.35f};
    float gridAlpha = 0.5f;

    // HDRI environment map (Presentation mode reflections)
    std::string hdriPath;
    float hdriIntensity = 1.0f;
    float hdriRotation = 0.0f;  // degrees, 0-360

    // Selection
    glm::vec3 wireframeColor{0.2f, 1.0f, 0.2f};
    float selectionBrightness = 1.3f;

    // Lighting
    float ambientStrength = 0.14f;
    float specularStrength = 0.55f;
    float specularShininess = 48.0f;

    // Optics display
    bool showFocalPoints = true;

    // Bloom (Presentation mode)
    float bloomThreshold = 0.3f;
    float bloomIntensity = 1.2f;
    int bloomBlurPasses = 5;

    // Snapping
    bool snapToGrid = false;
    float gridSpacing = 25.0f;
    bool snapToElement = false;
    float elementSnapRadius = 2.0f;
    bool snapToBeam = false;
    float beamSnapRadius = 3.0f;
    bool autoOrientToBeam = true;

    SceneStyle() { resetToDefaults(); }
    void resetToDefaults();
};

} // namespace opticsketch
