#include "ui/properties_panel.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "elements/annotation.h"
#include "elements/measurement.h"
#include "render/beam.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <cstring>
#include <cmath>

namespace opticsketch {

static const char* elementTypeLabel(ElementType type) {
    switch (type) {
        case ElementType::Laser:         return "Laser";
        case ElementType::Mirror:        return "Mirror";
        case ElementType::Lens:          return "Lens";
        case ElementType::BeamSplitter:  return "Beam Splitter";
        case ElementType::Detector:      return "Detector";
        case ElementType::Filter:        return "Filter";
        case ElementType::Aperture:      return "Aperture";
        case ElementType::Prism:         return "Prism";
        case ElementType::PrismRA:       return "Prism RA";
        case ElementType::Grating:       return "Grating";
        case ElementType::FiberCoupler:  return "Fiber Coupler";
        case ElementType::Screen:        return "Screen";
        case ElementType::Mount:         return "Mount";
        case ElementType::ImportedMesh:  return "Mesh";
        default:                         return "?";
    }
}

void PropertiesPanel::render(Scene* scene) {
    if (!visible || !scene) return;

    if (!ImGui::Begin("Properties", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    size_t selCount = scene->getSelectionCount();

    if (selCount == 0) {
        ImGui::TextDisabled("No selection");
        ImGui::TextDisabled("Select an object in the viewport or outliner.");
        ImGui::End();
        return;
    }

    // --- Multi-select panel ---
    if (selCount > 1) {
        ImGui::Text("%zu items selected", selCount);
        ImGui::Separator();

        auto selElems = scene->getSelectedElements();
        auto selBeams = scene->getSelectedBeams();

        if (!selElems.empty())
            ImGui::Text("Elements: %zu", selElems.size());
        if (!selBeams.empty())
            ImGui::Text("Beams: %zu", selBeams.size());

        ImGui::Spacing();

        // Batch state toggles for selected elements
        if (!selElems.empty()) {
            ImGui::Text("Batch Properties (Elements)");
            ImGui::Separator();

            // Determine mixed state for checkboxes
            bool allVisible = true, anyVisible = false;
            bool allLocked = true, anyLocked = false;
            int commonLayer = selElems[0]->layer;
            bool layerMixed = false;
            for (auto* e : selElems) {
                if (e->visible) anyVisible = true; else allVisible = false;
                if (e->locked) anyLocked = true; else allLocked = false;
                if (e->layer != commonLayer) layerMixed = true;
            }

            bool visVal = anyVisible;
            if (ImGui::Checkbox("Visible##batch", &visVal)) {
                for (auto* e : selElems) e->visible = visVal;
            }
            if (allVisible != anyVisible) { ImGui::SameLine(); ImGui::TextDisabled("(mixed)"); }

            bool lockVal = anyLocked;
            if (ImGui::Checkbox("Locked##batch", &lockVal)) {
                for (auto* e : selElems) e->locked = lockVal;
            }
            if (allLocked != anyLocked) { ImGui::SameLine(); ImGui::TextDisabled("(mixed)"); }

            int layerVal = commonLayer;
            if (layerMixed) ImGui::TextDisabled("Layer: (mixed)");
            else if (ImGui::DragInt("Layer##batch", &layerVal, 1, 0, 255)) {
                for (auto* e : selElems) e->layer = layerVal;
            }
        }

        ImGui::End();
        return;
    }

    // --- Single selection ---
    Element* elem = scene->getSelectedElement();
    Beam* beam = scene->getSelectedBeam();

    if (elem) {
        // Refresh buffer when selection changes
        static std::string s_lastId;
        static char labelBuf[256];
        if (elem->id != s_lastId) {
            s_lastId = elem->id;
            strncpy(labelBuf, elem->label.c_str(), sizeof(labelBuf) - 1);
            labelBuf[sizeof(labelBuf) - 1] = '\0';
        }

        ImGui::Text("Element");
        ImGui::Separator();
        if (ImGui::InputText("Label", labelBuf, sizeof(labelBuf)))
            elem->label = labelBuf;

        ImGui::Text("Type: %s", elementTypeLabel(elem->type));
        ImGui::Text("ID: %s", elem->id.c_str());
        ImGui::Spacing();

        ImGui::Text("Transform");
        ImGui::Separator();
        ImGui::DragFloat3("Position", &elem->transform.position.x, 0.1f, -1e6f, 1e6f, "%.3f");

        glm::vec3 eulerDeg = glm::degrees(glm::eulerAngles(elem->transform.rotation));
        if (ImGui::DragFloat3("Rotation (deg)", &eulerDeg.x, 1.0f, -360.0f, 360.0f, "%.1f"))
            elem->transform.rotation = glm::quat(glm::radians(eulerDeg));

        ImGui::DragFloat3("Scale", &elem->transform.scale.x, 0.01f, 0.001f, 1e6f, "%.3f");
        ImGui::Spacing();

        ImGui::Text("State");
        ImGui::Separator();
        ImGui::Checkbox("Visible", &elem->visible);
        ImGui::Checkbox("Show Label", &elem->showLabel);
        ImGui::Checkbox("Locked", &elem->locked);
        ImGui::DragInt("Layer", &elem->layer, 1, 0, 255);
        ImGui::Spacing();

        // --- Optical Properties ---
        if (ImGui::CollapsingHeader("Optical Properties")) {
            static const char* opticalTypeNames[] = {
                "Source", "Mirror", "Lens", "Splitter", "Absorber", "Prism", "Grating", "Passive",
                "Filter", "Aperture", "Fiber Coupler"
            };
            int otIdx = static_cast<int>(elem->optics.opticalType);
            if (ImGui::Combo("Optical Type", &otIdx, opticalTypeNames, 11)) {
                elem->optics.opticalType = static_cast<OpticalType>(otIdx);
            }

            ImGui::DragFloat("IOR##optics", &elem->optics.ior, 0.01f, 1.0f, 3.0f, "%.4f");
            ImGui::SliderFloat("Reflectivity##optics", &elem->optics.reflectivity, 0.0f, 1.0f, "%.3f");
            ImGui::SliderFloat("Transmissivity##optics", &elem->optics.transmissivity, 0.0f, 1.0f, "%.3f");

            if (elem->optics.opticalType == OpticalType::Lens) {
                ImGui::DragFloat("Focal Length (mm)##optics", &elem->optics.focalLength, 0.5f, 1.0f, 10000.0f, "%.1f");
                ImGui::DragFloat("Curvature R1 (mm)##optics", &elem->optics.curvatureR1, 1.0f, -10000.0f, 10000.0f, "%.1f");
                ImGui::DragFloat("Curvature R2 (mm)##optics", &elem->optics.curvatureR2, 1.0f, -10000.0f, 10000.0f, "%.1f");
                ImGui::Spacing();
                // Lensmaker's equation: 1/f = (n-1) * (1/R1 - 1/R2)
                float n = elem->optics.ior;
                float r1 = elem->optics.curvatureR1;
                float r2 = elem->optics.curvatureR2;
                if (std::abs(r1) > 0.01f && std::abs(r2) > 0.01f) {
                    float invF = (n - 1.0f) * (1.0f / r1 - 1.0f / r2);
                    if (std::abs(invF) > 1e-8f) {
                        float fComputed = 1.0f / invF;
                        ImGui::Text("Computed f: %.2f mm", fComputed);
                    } else {
                        ImGui::Text("Computed f: infinity");
                    }
                }
            }

            if (elem->optics.opticalType == OpticalType::Filter) {
                ImGui::ColorEdit3("Filter Color##optics", &elem->optics.filterColor.x);
            }

            if (elem->optics.opticalType == OpticalType::Aperture) {
                ImGui::SliderFloat("Opening Size##optics", &elem->optics.apertureDiameter, 0.05f, 0.95f, "%.2f");
            }

            if (elem->optics.opticalType == OpticalType::Grating) {
                ImGui::DragFloat("Line Density (lines/mm)##optics", &elem->optics.gratingLineDensity, 10.0f, 100.0f, 2400.0f, "%.0f");
            }

            // Dispersion for refractive elements
            if (elem->optics.opticalType == OpticalType::Lens ||
                elem->optics.opticalType == OpticalType::Prism) {
                // Display in units of 1e-15 for readability
                float cauchyBscaled = elem->optics.cauchyB * 1e15f;
                if (ImGui::DragFloat("Cauchy B (x1e-15 m^2)##optics", &cauchyBscaled, 0.1f, 0.0f, 20.0f, "%.1f")) {
                    elem->optics.cauchyB = cauchyBscaled * 1e-15f;
                }
                ImGui::TextDisabled("0 = no dispersion, 4.2 = BK7 glass");
            }

            // Source-specific properties
            if (elem->optics.opticalType == OpticalType::Source) {
                ImGui::Separator();
                ImGui::Text("Source Settings");
                ImGui::DragInt("Ray Count##source", &elem->optics.sourceRayCount, 2, 1, 21);
                // Force odd for symmetric spread
                if (elem->optics.sourceRayCount > 1 && elem->optics.sourceRayCount % 2 == 0)
                    elem->optics.sourceRayCount++;
                ImGui::DragFloat("Beam Width (mm)##source", &elem->optics.sourceBeamWidth, 0.1f, 0.0f, 50.0f, "%.1f");
                ImGui::Checkbox("White Light##source", &elem->optics.sourceIsWhiteLight);
                ImGui::TextDisabled("White light emits 7 wavelengths for dispersion");
            }
        }

        // --- Material Properties ---
        if (ImGui::CollapsingHeader("Material")) {
            ImGui::SliderFloat("Metallic##mat", &elem->material.metallic, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Roughness##mat", &elem->material.roughness, 0.0f, 1.0f, "%.2f");
            ImGui::SliderFloat("Transparency##mat", &elem->material.transparency, 0.0f, 1.0f, "%.2f");
            ImGui::DragFloat("Fresnel IOR##mat", &elem->material.fresnelIOR, 0.01f, 1.0f, 3.0f, "%.3f");
        }
    } else if (beam) {
        // Single beam properties
        static std::string s_lastBeamId;
        static char beamLabelBuf[256];
        if (beam->id != s_lastBeamId) {
            s_lastBeamId = beam->id;
            strncpy(beamLabelBuf, beam->label.c_str(), sizeof(beamLabelBuf) - 1);
            beamLabelBuf[sizeof(beamLabelBuf) - 1] = '\0';
        }

        ImGui::Text("Beam");
        ImGui::Separator();
        if (ImGui::InputText("Label##beam", beamLabelBuf, sizeof(beamLabelBuf)))
            beam->label = beamLabelBuf;

        ImGui::Text("ID: %s", beam->id.c_str());
        ImGui::Spacing();

        ImGui::Text("Geometry");
        ImGui::Separator();
        ImGui::DragFloat3("Start", &beam->start.x, 0.1f, -1e6f, 1e6f, "%.3f");
        ImGui::DragFloat3("End", &beam->end.x, 0.1f, -1e6f, 1e6f, "%.3f");
        ImGui::Spacing();

        ImGui::Text("Appearance");
        ImGui::Separator();
        ImGui::ColorEdit3("Color##beam", &beam->color.x);
        ImGui::DragFloat("Width##beam", &beam->width, 0.1f, 0.5f, 20.0f, "%.1f");
        ImGui::Spacing();

        ImGui::Text("State");
        ImGui::Separator();
        ImGui::Checkbox("Visible##beam", &beam->visible);
        ImGui::DragInt("Layer##beam", &beam->layer, 1, 0, 255);
        ImGui::Spacing();

        // --- Gaussian Beam section ---
        ImGui::Text("Gaussian Beam");
        ImGui::Separator();
        ImGui::Checkbox("Enable Gaussian##beam", &beam->isGaussian);

        if (beam->isGaussian) {
            // Waist w0 displayed in mm (stored in meters)
            float waistMM = beam->waistW0 * 1000.0f;
            if (ImGui::DragFloat("Waist w0 (mm)##beam", &waistMM, 0.01f, 0.001f, 100.0f, "%.3f"))
                beam->waistW0 = waistMM / 1000.0f;

            // Wavelength displayed in nm (stored in meters)
            float wavelengthNM = beam->wavelength * 1e9f;
            if (ImGui::DragFloat("Wavelength (nm)##beam", &wavelengthNM, 1.0f, 100.0f, 2000.0f, "%.0f"))
                beam->wavelength = wavelengthNM * 1e-9f;

            ImGui::SliderFloat("Waist Position##beam", &beam->waistPosition, 0.0f, 1.0f, "%.2f");

            ImGui::Spacing();
            ImGui::Text("Computed Values");
            ImGui::Separator();

            float zR = beam->getRayleighRange();
            float divergence = beam->getDivergenceAngle();
            float beamLength = beam->getLength();
            float endRadius = beam->beamRadiusAt(beamLength * (1.0f - beam->waistPosition));

            ImGui::Text("Rayleigh range: %.3f mm", zR * 1000.0f);
            ImGui::Text("Divergence: %.3f mrad", divergence * 1000.0f);
            ImGui::Text("Radius at end: %.3f mm", endRadius * 1000.0f);
        }
    } else {
        // Check for selected annotation
        opticsketch::Annotation* ann = scene->getSelectedAnnotation();
        if (ann) {
            static std::string s_lastAnnId;
            static char annLabelBuf[256];
            static char annTextBuf[1024];
            if (ann->id != s_lastAnnId) {
                s_lastAnnId = ann->id;
                strncpy(annLabelBuf, ann->label.c_str(), sizeof(annLabelBuf) - 1);
                annLabelBuf[sizeof(annLabelBuf) - 1] = '\0';
                strncpy(annTextBuf, ann->text.c_str(), sizeof(annTextBuf) - 1);
                annTextBuf[sizeof(annTextBuf) - 1] = '\0';
            }

            ImGui::Text("Annotation");
            ImGui::Separator();
            if (ImGui::InputText("Label##ann", annLabelBuf, sizeof(annLabelBuf)))
                ann->label = annLabelBuf;

            ImGui::Text("ID: %s", ann->id.c_str());
            ImGui::Spacing();

            ImGui::Text("Text");
            ImGui::Separator();
            if (ImGui::InputTextMultiline("##anntext", annTextBuf, sizeof(annTextBuf), ImVec2(-1, 80)))
                ann->text = annTextBuf;
            ImGui::Spacing();

            ImGui::Text("Position");
            ImGui::Separator();
            ImGui::DragFloat3("Position##ann", &ann->position.x, 0.1f, -1e6f, 1e6f, "%.3f");
            ImGui::Spacing();

            ImGui::Text("Appearance");
            ImGui::Separator();
            ImGui::ColorEdit3("Color##ann", &ann->color.x);
            ImGui::DragFloat("Font Size##ann", &ann->fontSize, 0.5f, 6.0f, 72.0f, "%.0f");
            ImGui::Spacing();

            ImGui::Text("State");
            ImGui::Separator();
            ImGui::Checkbox("Visible##ann", &ann->visible);
            ImGui::DragInt("Layer##ann", &ann->layer, 1, 0, 255);
        } else {
            // Check for selected measurement
            opticsketch::Measurement* meas = scene->getSelectedMeasurement();
            if (meas) {
                static std::string s_lastMeasId;
                static char measLabelBuf[256];
                if (meas->id != s_lastMeasId) {
                    s_lastMeasId = meas->id;
                    strncpy(measLabelBuf, meas->label.c_str(), sizeof(measLabelBuf) - 1);
                    measLabelBuf[sizeof(measLabelBuf) - 1] = '\0';
                }

                ImGui::Text("Measurement");
                ImGui::Separator();
                if (ImGui::InputText("Label##meas", measLabelBuf, sizeof(measLabelBuf)))
                    meas->label = measLabelBuf;

                ImGui::Text("ID: %s", meas->id.c_str());
                ImGui::Spacing();

                ImGui::Text("Points");
                ImGui::Separator();
                ImGui::DragFloat3("Start##meas", &meas->startPoint.x, 0.1f, -1e6f, 1e6f, "%.3f");
                ImGui::DragFloat3("End##meas", &meas->endPoint.x, 0.1f, -1e6f, 1e6f, "%.3f");
                ImGui::Spacing();

                ImGui::Text("Distance: %.3f", meas->getDistance());
                ImGui::Spacing();

                ImGui::Text("Appearance");
                ImGui::Separator();
                ImGui::ColorEdit3("Color##meas", &meas->color.x);
                ImGui::DragFloat("Font Size##meas", &meas->fontSize, 0.5f, 6.0f, 72.0f, "%.0f");
                ImGui::Spacing();

                ImGui::Text("State");
                ImGui::Separator();
                ImGui::Checkbox("Visible##meas", &meas->visible);
                ImGui::DragInt("Layer##meas", &meas->layer, 1, 0, 255);
            } else {
                ImGui::TextDisabled("No selection");
                ImGui::TextDisabled("Select an object in the viewport or outliner.");
            }
        }
    }

    ImGui::End();
}

} // namespace opticsketch
