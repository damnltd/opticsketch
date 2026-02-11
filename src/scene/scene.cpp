#include "scene/scene.h"
#include "render/beam.h"
#include <algorithm>
#include <string>

namespace opticsketch {

Scene::Scene() = default;
Scene::~Scene() = default;

// Ensure id is unique in the scene; if it clashes, append _2, _3, ...
static void ensureUniqueId(Scene* scene, Element* element) {
    std::string base = element->id;
    int suffix = 2;
    while (scene->getElement(element->id)) {
        element->id = base + "_" + std::to_string(suffix);
        ++suffix;
    }
}

// Ensure label is unique among existing elements; if it clashes, append " 2", " 3", ...
static void ensureUniqueLabel(const std::vector<std::unique_ptr<Element>>& elements, Element* element) {
    auto labelExists = [&elements](const std::string& label) {
        return std::any_of(elements.begin(), elements.end(),
            [&label](const std::unique_ptr<Element>& e) { return e->label == label; });
    };
    std::string base = element->label;
    int suffix = 2;
    while (labelExists(element->label)) {
        element->label = base + " " + std::to_string(suffix);
        ++suffix;
    }
}

void Scene::addElement(std::unique_ptr<Element> element) {
    if (!element) return;
    Element* ptr = element.get();
    ensureUniqueId(this, ptr);
    ensureUniqueLabel(elements, ptr);
    elements.push_back(std::move(element));
}

bool Scene::removeElement(const std::string& id) {
    auto it = std::find_if(elements.begin(), elements.end(),
        [&id](const std::unique_ptr<Element>& elem) {
            return elem->id == id;
        });
    
    if (it != elements.end()) {
        if (selectedElement == it->get()) {
            selectedElement = nullptr;
        }
        elements.erase(it);
        return true;
    }
    return false;
}

Element* Scene::getElement(const std::string& id) {
    auto it = std::find_if(elements.begin(), elements.end(),
        [&id](const std::unique_ptr<Element>& elem) {
            return elem->id == id;
        });
    
    return (it != elements.end()) ? it->get() : nullptr;
}

void Scene::addBeam(std::unique_ptr<Beam> beam) {
    if (!beam) return;
    beams.push_back(std::move(beam));
}

bool Scene::removeBeam(const std::string& id) {
    auto it = std::find_if(beams.begin(), beams.end(),
        [&id](const std::unique_ptr<Beam>& beam) {
            return beam->id == id;
        });

    if (it != beams.end()) {
        if (selectedBeam == it->get()) {
            selectedBeam = nullptr;
        }
        beams.erase(it);
        return true;
    }
    return false;
}

Beam* Scene::getBeam(const std::string& id) {
    auto it = std::find_if(beams.begin(), beams.end(),
        [&id](const std::unique_ptr<Beam>& beam) {
            return beam->id == id;
        });
    
    return (it != beams.end()) ? it->get() : nullptr;
}

void Scene::clear() {
    elements.clear();
    beams.clear();
    selectedElement = nullptr;
    selectedBeam = nullptr;
}

void Scene::selectElement(const std::string& id) {
    selectedElement = getElement(id);
    selectedBeam = nullptr;  // mutual exclusion
}

void Scene::selectBeam(const std::string& id) {
    selectedBeam = getBeam(id);
    selectedElement = nullptr;  // mutual exclusion
}

void Scene::deselectAll() {
    selectedElement = nullptr;
    selectedBeam = nullptr;
}

bool Scene::isSelected(const std::string& id) const {
    return (selectedElement && selectedElement->id == id) ||
           (selectedBeam && selectedBeam->id == id);
}

} // namespace opticsketch
