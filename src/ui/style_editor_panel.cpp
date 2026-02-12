#include "ui/style_editor_panel.h"
#include "style/scene_style.h"

namespace opticsketch {

static const char* elementTypeNames[] = {
    "Laser", "Mirror", "Lens", "Beam Splitter", "Detector",
    "Filter", "Aperture", "Prism", "Prism RA", "Grating",
    "Fiber Coupler", "Screen", "Mount", "Imported Mesh"
};

void StyleEditorPanel::render(SceneStyle* style) {
    if (!visible || !style) return;

    if (!ImGui::Begin("Style Editor", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    // Element Colors
    if (ImGui::CollapsingHeader("Element Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        for (int i = 0; i < kElementTypeCount; i++) {
            ImGui::ColorEdit3(elementTypeNames[i], &style->elementColors[i].x,
                              ImGuiColorEditFlags_NoInputs);
        }
    }

    // Viewport
    if (ImGui::CollapsingHeader("Viewport", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit3("Background", &style->bgColor.x, ImGuiColorEditFlags_NoInputs);
        ImGui::ColorEdit3("Grid Color", &style->gridColor.x, ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Grid Alpha", &style->gridAlpha, 0.0f, 1.0f, "%.2f");
    }

    // Selection
    if (ImGui::CollapsingHeader("Selection")) {
        ImGui::ColorEdit3("Wireframe", &style->wireframeColor.x, ImGuiColorEditFlags_NoInputs);
        ImGui::SliderFloat("Brightness", &style->selectionBrightness, 1.0f, 2.0f, "%.2f");
    }

    // Lighting
    if (ImGui::CollapsingHeader("Lighting")) {
        ImGui::SliderFloat("Ambient", &style->ambientStrength, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Specular", &style->specularStrength, 0.0f, 2.0f, "%.2f");
        ImGui::SliderFloat("Shininess", &style->specularShininess, 1.0f, 256.0f, "%.0f");
    }

    // Snapping
    if (ImGui::CollapsingHeader("Snapping")) {
        ImGui::Checkbox("Snap to Grid", &style->snapToGrid);
        ImGui::DragFloat("Grid Spacing", &style->gridSpacing, 1.0f, 1.0f, 200.0f, "%.0f mm");
        ImGui::Checkbox("Snap to Element", &style->snapToElement);
        ImGui::DragFloat("Snap Radius", &style->elementSnapRadius, 0.1f, 0.1f, 50.0f, "%.1f");
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset to Defaults")) {
        style->resetToDefaults();
    }

    ImGui::End();
}

} // namespace opticsketch
