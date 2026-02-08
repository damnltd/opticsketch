#include "ui/properties_panel.h"
#include "scene/scene.h"
#include "elements/element.h"
#include <glm/gtc/quaternion.hpp>
#include <glm/trigonometric.hpp>
#include <cstring>

namespace opticsketch {

static const char* elementTypeLabel(ElementType type) {
    switch (type) {
        case ElementType::Laser:         return "Laser";
        case ElementType::Mirror:        return "Mirror";
        case ElementType::Lens:          return "Lens";
        case ElementType::BeamSplitter:  return "Beam Splitter";
        case ElementType::Detector:      return "Detector";
        default:                         return "?";
    }
}

void PropertiesPanel::render(Scene* scene) {
    if (!visible || !scene) return;

    if (!ImGui::Begin("Properties", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    Element* elem = scene->getSelectedElement();
    if (!elem) {
        ImGui::TextDisabled("No selection");
        ImGui::TextDisabled("Select an object in the viewport or outliner.");
        ImGui::End();
        return;
    }

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
    ImGui::Checkbox("Locked", &elem->locked);
    ImGui::DragInt("Layer", &elem->layer, 1, 0, 255);

    ImGui::End();
}

} // namespace opticsketch
