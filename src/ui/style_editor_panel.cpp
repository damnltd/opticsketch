#include "ui/style_editor_panel.h"
#include "style/scene_style.h"
#include <tinyfiledialogs.h>
#include <string>

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

    // Render Mode
    if (ImGui::CollapsingHeader("Render Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        static const char* renderModeNames[] = { "Standard", "Schematic", "Presentation" };
        int modeIdx = static_cast<int>(style->renderMode);
        if (ImGui::Combo("Mode", &modeIdx, renderModeNames, 3)) {
            style->renderMode = static_cast<RenderMode>(modeIdx);
        }
        ImGui::TextDisabled("Standard: Blinn-Phong 3D shading");
        ImGui::TextDisabled("Schematic: Flat colors, dark outlines, white bg");
        ImGui::TextDisabled("Presentation: Materials + bloom + HDRI");

        if (style->renderMode == RenderMode::Presentation) {
            ImGui::Spacing();
            ImGui::SliderFloat("Bloom Threshold", &style->bloomThreshold, 0.1f, 2.0f, "%.2f");
            ImGui::SliderFloat("Bloom Intensity", &style->bloomIntensity, 0.0f, 3.0f, "%.2f");
            ImGui::SliderInt("Bloom Passes", &style->bloomBlurPasses, 1, 15);

            ImGui::Separator();
            ImGui::Text("HDRI Environment");
            std::string hdriLabel = style->hdriPath.empty() ? "None" :
                style->hdriPath.substr(style->hdriPath.find_last_of("/\\") + 1);
            ImGui::Text("File: %s", hdriLabel.c_str());
            ImGui::SameLine();
            if (ImGui::Button("Load...")) {
                const char* filters[] = { "*.hdr", "*.HDR" };
                const char* path = tinyfd_openFileDialog(
                    "Load HDRI Environment Map", "", 2, filters, "HDR Files (*.hdr)", 0);
                if (path) {
                    style->hdriPath = path;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear") && !style->hdriPath.empty()) {
                style->hdriPath.clear();
            }
            if (!style->hdriPath.empty()) {
                ImGui::SliderFloat("Env Intensity", &style->hdriIntensity, 0.0f, 5.0f, "%.2f");
                ImGui::SliderFloat("Env Rotation", &style->hdriRotation, 0.0f, 360.0f, "%.0f deg");
            }
        }
    }

    // Optics Display
    if (ImGui::CollapsingHeader("Optics Display")) {
        ImGui::Checkbox("Show Focal Points", &style->showFocalPoints);
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
        static const char* bgModeNames[] = { "Solid Color", "Gradient" };
        int bgModeIdx = static_cast<int>(style->bgMode);
        if (ImGui::Combo("Background", &bgModeIdx, bgModeNames, 2)) {
            style->bgMode = static_cast<BackgroundMode>(bgModeIdx);
        }

        if (style->bgMode == BackgroundMode::Solid) {
            ImGui::ColorEdit3("Bg Color", &style->bgColor.x, ImGuiColorEditFlags_NoInputs);
        } else {
            ImGui::ColorEdit3("Top Color", &style->bgGradientTop.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit3("Bottom Color", &style->bgGradientBottom.x, ImGuiColorEditFlags_NoInputs);
        }

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
