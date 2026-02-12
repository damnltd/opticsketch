#pragma once

#include <glm/glm.hpp>
#include <array>

namespace opticsketch {

static constexpr int kElementTypeCount = 14;

struct SceneStyle {
    // Element colors (indexed by (int)ElementType)
    std::array<glm::vec3, kElementTypeCount> elementColors;

    // Viewport
    glm::vec3 bgColor{0.05f, 0.05f, 0.06f};
    glm::vec3 gridColor{0.3f, 0.3f, 0.35f};
    float gridAlpha = 0.5f;

    // Selection
    glm::vec3 wireframeColor{0.2f, 1.0f, 0.2f};
    float selectionBrightness = 1.3f;

    // Lighting
    float ambientStrength = 0.14f;
    float specularStrength = 0.55f;
    float specularShininess = 48.0f;

    // Snapping
    bool snapToGrid = false;
    float gridSpacing = 25.0f;
    bool snapToElement = false;
    float elementSnapRadius = 2.0f;

    SceneStyle() { resetToDefaults(); }
    void resetToDefaults();
};

} // namespace opticsketch
