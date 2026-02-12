#include "ui/outliner_panel.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "elements/annotation.h"
#include "elements/measurement.h"
#include "render/beam.h"
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>

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

// Item types in the flat list
enum class OutlinerItemType { Element, Beam, Annotation, Measurement };

struct OutlinerItem {
    std::string id;
    OutlinerItemType type;
};

void OutlinerPanel::render(Scene* scene) {
    if (!visible || !scene) return;

    if (!ImGui::Begin("Outliner", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted("Scene");
    ImGui::Separator();

    const auto& elements = scene->getElements();
    const auto& beams = scene->getBeams();
    const auto& annotations = scene->getAnnotations();
    const auto& measurements = scene->getMeasurements();

    if (elements.empty() && beams.empty() && annotations.empty() && measurements.empty()) {
        ImGui::TextDisabled("(no objects)");
        lastClickedIndex = -1;
        ImGui::End();
        return;
    }

    // Build flat item list for range selection
    std::vector<OutlinerItem> items;
    items.reserve(elements.size() + beams.size() + annotations.size() + measurements.size());
    for (const auto& e : elements)      if (e) items.push_back({e->id, OutlinerItemType::Element});
    for (const auto& b : beams)         if (b) items.push_back({b->id, OutlinerItemType::Beam});
    for (const auto& a : annotations)   if (a) items.push_back({a->id, OutlinerItemType::Annotation});
    for (const auto& m : measurements)  if (m) items.push_back({m->id, OutlinerItemType::Measurement});

    // Clamp lastClickedIndex if scene changed
    if (lastClickedIndex >= static_cast<int>(items.size())) lastClickedIndex = -1;

    // Helper: select a single item by its OutlinerItem (additive or not)
    auto selectItem = [&](const OutlinerItem& item, bool additive) {
        switch (item.type) {
            case OutlinerItemType::Element:     scene->selectElement(item.id, additive); break;
            case OutlinerItemType::Beam:        scene->selectBeam(item.id, additive); break;
            case OutlinerItemType::Annotation:  scene->selectAnnotation(item.id, additive); break;
            case OutlinerItemType::Measurement: scene->selectMeasurement(item.id, additive); break;
        }
    };

    int flatIndex = 0;

    // --- Elements ---
    for (const auto& elem : elements) {
        if (!elem) continue;
        int myIndex = flatIndex++;

        bool isSelected = scene->isSelected(elem->id);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        }
        bool clicked = ImGui::Selectable(
            elem->label.empty() ? elem->id.c_str() : elem->label.c_str(),
            isSelected, 0, ImVec2(0, 0)
        );
        if (isSelected) ImGui::PopStyleColor(2);

        if (clicked) {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift && lastClickedIndex >= 0 && lastClickedIndex < static_cast<int>(items.size())) {
                // Range select: select all items between lastClicked and current
                int lo = std::min(lastClickedIndex, myIndex);
                int hi = std::max(lastClickedIndex, myIndex);
                scene->deselectAll();
                for (int i = lo; i <= hi; i++) selectItem(items[i], true);
            } else if (io.KeyCtrl) {
                scene->toggleSelect(elem->id);
            } else {
                scene->selectElement(elem->id);
            }
            lastClickedIndex = myIndex;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", elem->label.c_str());
            ImGui::TextDisabled("%s | %s", elementTypeLabel(elem->type), elem->id.c_str());
            ImGui::EndTooltip();
        }
    }

    // --- Beams ---
    for (const auto& beam : beams) {
        if (!beam) continue;
        int myIndex = flatIndex++;

        bool isSelected = scene->isSelected(beam->id);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        }
        bool clicked = ImGui::Selectable(
            beam->label.empty() ? beam->id.c_str() : beam->label.c_str(),
            isSelected, 0, ImVec2(0, 0)
        );
        if (isSelected) ImGui::PopStyleColor(2);

        if (clicked) {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift && lastClickedIndex >= 0 && lastClickedIndex < static_cast<int>(items.size())) {
                int lo = std::min(lastClickedIndex, myIndex);
                int hi = std::max(lastClickedIndex, myIndex);
                scene->deselectAll();
                for (int i = lo; i <= hi; i++) selectItem(items[i], true);
            } else if (io.KeyCtrl) {
                scene->toggleSelect(beam->id);
            } else {
                scene->selectBeam(beam->id);
            }
            lastClickedIndex = myIndex;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", beam->label.c_str());
            ImGui::TextDisabled("Beam | %s", beam->id.c_str());
            ImGui::EndTooltip();
        }
    }

    // --- Annotations ---
    for (const auto& ann : annotations) {
        if (!ann) continue;
        int myIndex = flatIndex++;

        bool isSelected = scene->isSelected(ann->id);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        }
        bool clicked = ImGui::Selectable(
            ann->label.empty() ? ann->id.c_str() : ann->label.c_str(),
            isSelected, 0, ImVec2(0, 0)
        );
        if (isSelected) ImGui::PopStyleColor(2);

        if (clicked) {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift && lastClickedIndex >= 0 && lastClickedIndex < static_cast<int>(items.size())) {
                int lo = std::min(lastClickedIndex, myIndex);
                int hi = std::max(lastClickedIndex, myIndex);
                scene->deselectAll();
                for (int i = lo; i <= hi; i++) selectItem(items[i], true);
            } else if (io.KeyCtrl) {
                scene->toggleSelect(ann->id);
            } else {
                scene->selectAnnotation(ann->id);
            }
            lastClickedIndex = myIndex;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", ann->label.c_str());
            ImGui::TextDisabled("Annotation | %s", ann->id.c_str());
            ImGui::EndTooltip();
        }
    }

    // --- Measurements ---
    for (const auto& meas : measurements) {
        if (!meas) continue;
        int myIndex = flatIndex++;

        bool isSelected = scene->isSelected(meas->id);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
        }
        char measLabel[128];
        snprintf(measLabel, sizeof(measLabel), "%s (%.1f mm)",
                 meas->label.empty() ? meas->id.c_str() : meas->label.c_str(),
                 meas->getDistance());
        bool clicked = ImGui::Selectable(measLabel, isSelected, 0, ImVec2(0, 0));
        if (isSelected) ImGui::PopStyleColor(2);

        if (clicked) {
            const ImGuiIO& io = ImGui::GetIO();
            if (io.KeyShift && lastClickedIndex >= 0 && lastClickedIndex < static_cast<int>(items.size())) {
                int lo = std::min(lastClickedIndex, myIndex);
                int hi = std::max(lastClickedIndex, myIndex);
                scene->deselectAll();
                for (int i = lo; i <= hi; i++) selectItem(items[i], true);
            } else if (io.KeyCtrl) {
                scene->toggleSelect(meas->id);
            } else {
                scene->selectMeasurement(meas->id);
            }
            lastClickedIndex = myIndex;
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text("%s", meas->label.c_str());
            ImGui::TextDisabled("Measurement | %s | %.1f mm", meas->id.c_str(), meas->getDistance());
            ImGui::EndTooltip();
        }
    }

    ImGui::End();
}

} // namespace opticsketch
