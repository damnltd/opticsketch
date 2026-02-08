#include "ui/outliner_panel.h"
#include "scene/scene.h"
#include "elements/element.h"

namespace opticsketch {

static const char* elementTypeLabel(ElementType type) {
    switch (type) {
        case ElementType::Laser:         return "Laser";
        case ElementType::Mirror:        return "Mirror";
        case ElementType::Lens:         return "Lens";
        case ElementType::BeamSplitter: return "Beam Splitter";
        case ElementType::Detector:     return "Detector";
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

    if (elements.empty()) {
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

            // Optional: show type as secondary text on same line
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("%s", elem->label.c_str());
                ImGui::TextDisabled("%s | %s", elementTypeLabel(elem->type), elem->id.c_str());
                ImGui::EndTooltip();
            }
        }
    }

    ImGui::End();
}

} // namespace opticsketch
