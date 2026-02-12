#include "ui/animation_export_panel.h"
#include "render/viewport.h"
#include "scene/scene.h"
#include "style/scene_style.h"
#include "render/beam.h"
#include <tinyfiledialogs.h>
#include <sstream>
#include <cstring>

namespace opticsketch {

void AnimationExportPanel::render(Viewport* viewport, Scene* scene, SceneStyle* style) {
    if (!visible) return;

    // Check ffmpeg on first render
    if (!checkedFFmpeg) {
        ffmpegAvailable = isFFmpegAvailable();
        checkedFFmpeg = true;
    }

    ImGui::SetNextWindowSize(ImVec2(420, 480), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Export Animation", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    if (exportState.active) {
        // --- Progress View ---
        ImGui::Text("Exporting...");
        ImGui::Separator();

        float progress = static_cast<float>(exportState.currentFrame) /
                         static_cast<float>(exportState.totalFrames);
        ImGui::ProgressBar(progress, ImVec2(-1, 0));
        ImGui::Text("%s", exportState.statusText.c_str());

        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(-1, 0))) {
            exportState.cancelled = true;
        }
    } else {
        // --- Settings View ---

        // Animation Type
        static const char* typeNames[] = { "Turntable", "Beam Propagation", "Parameter Sweep" };
        int typeIdx = static_cast<int>(settings.type);
        if (ImGui::Combo("Animation Type", &typeIdx, typeNames, 3)) {
            settings.type = static_cast<AnimationType>(typeIdx);
        }

        ImGui::Separator();

        // Type-specific parameters
        if (settings.type == AnimationType::Turntable) {
            ImGui::Text("Turntable Settings");
            ImGui::DragFloat("Start Angle", &settings.turntable.startAngle, 1.0f, -360.0f, 360.0f, "%.0f deg");
            ImGui::DragFloat("End Angle", &settings.turntable.endAngle, 1.0f, -720.0f, 720.0f, "%.0f deg");
            ImGui::DragFloat("Elevation", &settings.turntable.elevation, 0.5f, -89.0f, 89.0f, "%.1f deg");
            ImGui::DragFloat("Distance", &settings.turntable.distance, 0.5f, 0.0f, 500.0f, "%.1f");
            ImGui::TextDisabled("Distance 0 = use current camera distance");
        } else if (settings.type == AnimationType::BeamPropagation) {
            ImGui::Text("Beam Propagation Settings");
            ImGui::Checkbox("All Beams", &settings.beamPropagation.allBeams);
            if (!settings.beamPropagation.allBeams) {
                // Show beam selector
                if (scene && ImGui::BeginCombo("Beam", settings.beamPropagation.beamId.c_str())) {
                    for (const auto& beam : scene->getBeams()) {
                        bool selected = (beam->id == settings.beamPropagation.beamId);
                        if (ImGui::Selectable(beam->label.c_str(), selected)) {
                            settings.beamPropagation.beamId = beam->id;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
        } else if (settings.type == AnimationType::ParameterSweep) {
            ImGui::Text("Parameter Sweep Settings");
            // Element selector
            if (scene && ImGui::BeginCombo("Element", settings.parameterSweep.elementId.c_str())) {
                for (const auto& elem : scene->getElements()) {
                    bool selected = (elem->id == settings.parameterSweep.elementId);
                    if (ImGui::Selectable(elem->label.c_str(), selected)) {
                        settings.parameterSweep.elementId = elem->id;
                    }
                }
                ImGui::EndCombo();
            }
            static const char* paramNames[] = { "Position X", "Position Y", "Position Z", "Rotation Y", "Focal Length" };
            ImGui::Combo("Parameter", &settings.parameterSweep.parameterIndex, paramNames, 5);
            ImGui::DragFloat("Start Value", &settings.parameterSweep.startValue, 0.1f);
            ImGui::DragFloat("End Value", &settings.parameterSweep.endValue, 0.1f);
        }

        ImGui::Separator();

        // Common settings
        ImGui::Text("Output Settings");

        // Resolution presets
        static const char* resPresets[] = { "720p (1280x720)", "1080p (1920x1080)", "Viewport Size" };
        static int resPresetIdx = 0;
        if (ImGui::Combo("Resolution", &resPresetIdx, resPresets, 3)) {
            switch (resPresetIdx) {
                case 0: settings.width = 1280; settings.height = 720; break;
                case 1: settings.width = 1920; settings.height = 1080; break;
                case 2:
                    if (viewport) {
                        settings.width = viewport->getWidth();
                        settings.height = viewport->getHeight();
                    }
                    break;
            }
        }
        ImGui::DragInt("Width", &settings.width, 1, 320, 3840);
        ImGui::DragInt("Height", &settings.height, 1, 240, 2160);

        ImGui::DragInt("Frame Count", &settings.frameCount, 1, 10, 1000);
        ImGui::DragInt("FPS", &settings.fps, 1, 1, 120);

        float duration = static_cast<float>(settings.frameCount) / static_cast<float>(settings.fps);
        ImGui::Text("Duration: %.1f seconds", duration);

        // Easing
        static const char* easingNames[] = { "Linear", "Ease In/Out", "Ease In", "Ease Out" };
        int easingIdx = static_cast<int>(settings.easing);
        if (ImGui::Combo("Easing", &easingIdx, easingNames, 4)) {
            settings.easing = static_cast<EasingFunction>(easingIdx);
        }

        // Output format
        static const char* formatNames[] = { "Image Sequence (PNG)", "Animated GIF", "MP4 Video" };
        int formatIdx = static_cast<int>(settings.format);
        if (ImGui::Combo("Format", &formatIdx, formatNames, 3)) {
            settings.format = static_cast<AnimationOutputFormat>(formatIdx);
        }

        if (settings.format == AnimationOutputFormat::MP4 && !ffmpegAvailable) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "ffmpeg not found - MP4 unavailable");
        }
        if (settings.format == AnimationOutputFormat::GIF && !ffmpegAvailable) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "ffmpeg not found - GIF may be unavailable");
        }

        // Output path
        ImGui::Text("Output: %s", settings.outputPath.empty() ? "(not set)" : settings.outputPath.c_str());
        if (ImGui::Button("Browse...")) {
            if (settings.format == AnimationOutputFormat::ImageSequence) {
                const char* path = tinyfd_selectFolderDialog("Select Output Folder", "");
                if (path) settings.outputPath = path;
            } else if (settings.format == AnimationOutputFormat::GIF) {
                const char* filters[] = { "*.gif" };
                const char* path = tinyfd_saveFileDialog("Save GIF", "animation.gif", 1, filters, "GIF Files");
                if (path) settings.outputPath = path;
            } else {
                const char* filters[] = { "*.mp4" };
                const char* path = tinyfd_saveFileDialog("Save MP4", "animation.mp4", 1, filters, "MP4 Files");
                if (path) settings.outputPath = path;
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        // Export button
        bool canExport = !settings.outputPath.empty();
        if (settings.format == AnimationOutputFormat::MP4 && !ffmpegAvailable) canExport = false;

        if (!canExport) ImGui::BeginDisabled();
        if (ImGui::Button("Export", ImVec2(-1, 30))) {
            beginAnimationExport(exportState, settings, viewport, scene);
        }
        if (!canExport) ImGui::EndDisabled();
    }

    ImGui::End();
}

bool AnimationExportPanel::advanceExport(Viewport* viewport, Scene* scene, SceneStyle* style) {
    if (!exportState.active) return false;

    if (exportState.cancelled || !advanceAnimationFrame(exportState, settings, viewport, scene, style)) {
        endAnimationExport(exportState, settings, viewport, scene);
        return false;
    }
    return true;
}

} // namespace opticsketch
