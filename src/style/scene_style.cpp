#include "style/scene_style.h"

namespace opticsketch {

void SceneStyle::resetToDefaults() {
    renderMode = RenderMode::Standard;

    // Element colors matching original hardcoded values
    elementColors[0]  = glm::vec3(1.0f, 0.2f, 0.2f);  // Laser
    elementColors[1]  = glm::vec3(0.8f, 0.8f, 0.9f);  // Mirror
    elementColors[2]  = glm::vec3(0.7f, 0.9f, 1.0f);  // Lens
    elementColors[3]  = glm::vec3(0.9f, 0.9f, 0.7f);  // BeamSplitter
    elementColors[4]  = glm::vec3(0.2f, 1.0f, 0.2f);  // Detector
    elementColors[5]  = glm::vec3(0.6f, 0.4f, 0.8f);  // Filter
    elementColors[6]  = glm::vec3(0.8f, 0.6f, 0.3f);  // Aperture
    elementColors[7]  = glm::vec3(0.5f, 0.8f, 0.9f);  // Prism
    elementColors[8]  = glm::vec3(0.5f, 0.8f, 0.9f);  // PrismRA
    elementColors[9]  = glm::vec3(0.7f, 0.5f, 0.3f);  // Grating
    elementColors[10] = glm::vec3(1.0f, 0.6f, 0.2f);  // FiberCoupler
    elementColors[11] = glm::vec3(0.3f, 0.8f, 0.3f);  // Screen
    elementColors[12] = glm::vec3(0.5f, 0.5f, 0.55f); // Mount
    elementColors[13] = glm::vec3(0.7f, 0.7f, 0.7f);  // ImportedMesh

    bgMode = BackgroundMode::Solid;
    bgColor = glm::vec3(0.05f, 0.05f, 0.06f);
    bgGradientTop = glm::vec3(0.12f, 0.12f, 0.15f);
    bgGradientBottom = glm::vec3(0.02f, 0.02f, 0.03f);
    gridColor = glm::vec3(0.3f, 0.3f, 0.35f);
    gridAlpha = 0.5f;

    hdriPath.clear();
    hdriIntensity = 1.0f;
    hdriRotation = 0.0f;
    wireframeColor = glm::vec3(0.2f, 1.0f, 0.2f);
    selectionBrightness = 1.3f;
    ambientStrength = 0.14f;
    specularStrength = 0.55f;
    specularShininess = 48.0f;

    snapToGrid = false;
    gridSpacing = 25.0f;
    snapToElement = false;
    elementSnapRadius = 2.0f;

    showFocalPoints = true;
    bloomThreshold = 0.8f;
    bloomIntensity = 1.0f;
    bloomBlurPasses = 5;
}

} // namespace opticsketch
