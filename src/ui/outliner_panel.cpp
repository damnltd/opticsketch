#include "ui/outliner_panel.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "render/beam.h"

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

void OutlinerPanel::render(Scene* scene) {
    if (!visible || !scene) return;

    if (!ImGui::Begin("Outliner", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Scene");
    ImGui::Separator();

    const auto& elements = scene->getElements();
    Element* selected = scene->getSelectedElement();

    if (elements.empty() && scene->getBeams().empty()) {
        ImGui::TextDisabled("(no objects)");
    } else {
        for (const auto& elem : elements) {
            if (!elem) continue;

            bool isSelected = (selected == elem.get());
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            }
            bool clicked = ImGui::Selectable(
                elem->label.empty() ? elem->id.c_str() : elem->label.c_str(),
                isSelected,
                0,
                ImVec2(0, 0)
            );
            if (isSelected) ImGui::PopStyleColor(2);

            if (clicked) {
                scene->selectElement(elem->id);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", elem->label.c_str());
                ImGui::TextDisabled("%s | %s", elementTypeLabel(elem->type), elem->id.c_str());
                ImGui::EndTooltip();
            }
        }

        // Beams
        const auto& beams = scene->getBeams();
        Beam* selectedBeam = scene->getSelectedBeam();
        for (const auto& beam : beams) {
            if (!beam) continue;

            bool isSelected = (selectedBeam == beam.get());
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            }
            bool clicked = ImGui::Selectable(
                beam->label.empty() ? beam->id.c_str() : beam->label.c_str(),
                isSelected,
                0,
                ImVec2(0, 0)
            );
            if (isSelected) ImGui::PopStyleColor(2);

            if (clicked) {
                scene->selectBeam(beam->id);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", beam->label.c_str());
                ImGui::TextDisabled("Beam | %s", beam->id.c_str());
                ImGui::EndTooltip();
            }
        }
    }

    ImGui::End();
}

} // namespace opticsketch
