#include "scene/scene.h"
#include "render/beam.h"
#include "elements/annotation.h"
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
        selectedIds.erase(id);
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
        selectedIds.erase(id);
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

void Scene::addAnnotation(std::unique_ptr<Annotation> annotation) {
    if (!annotation) return;
    annotations.push_back(std::move(annotation));
}

bool Scene::removeAnnotation(const std::string& id) {
    auto it = std::find_if(annotations.begin(), annotations.end(),
        [&id](const std::unique_ptr<Annotation>& ann) {
            return ann->id == id;
        });

    if (it != annotations.end()) {
        selectedIds.erase(id);
        annotations.erase(it);
        return true;
    }
    return false;
}

Annotation* Scene::getAnnotation(const std::string& id) {
    auto it = std::find_if(annotations.begin(), annotations.end(),
        [&id](const std::unique_ptr<Annotation>& ann) {
            return ann->id == id;
        });

    return (it != annotations.end()) ? it->get() : nullptr;
}

void Scene::clear() {
    elements.clear();
    beams.clear();
    annotations.clear();
    selectedIds.clear();
}

void Scene::selectElement(const std::string& id, bool additive) {
    if (!getElement(id)) return;
    if (!additive) selectedIds.clear();
    selectedIds.insert(id);
}

void Scene::selectBeam(const std::string& id, bool additive) {
    if (!getBeam(id)) return;
    if (!additive) selectedIds.clear();
    selectedIds.insert(id);
}

void Scene::selectAnnotation(const std::string& id, bool additive) {
    if (!getAnnotation(id)) return;
    if (!additive) selectedIds.clear();
    selectedIds.insert(id);
}

void Scene::toggleSelect(const std::string& id) {
    if (selectedIds.count(id))
        selectedIds.erase(id);
    else
        selectedIds.insert(id);
}

void Scene::deselectAll() {
    selectedIds.clear();
}

bool Scene::isSelected(const std::string& id) const {
    return selectedIds.count(id) > 0;
}

std::vector<Element*> Scene::getSelectedElements() const {
    std::vector<Element*> result;
    for (const auto& elem : elements) {
        if (selectedIds.count(elem->id))
            result.push_back(elem.get());
    }
    return result;
}

std::vector<Beam*> Scene::getSelectedBeams() const {
    std::vector<Beam*> result;
    for (const auto& beam : beams) {
        if (selectedIds.count(beam->id))
            result.push_back(beam.get());
    }
    return result;
}

Element* Scene::getSelectedElement() const {
    for (const auto& elem : elements) {
        if (selectedIds.count(elem->id))
            return elem.get();
    }
    return nullptr;
}

Beam* Scene::getSelectedBeam() const {
    for (const auto& beam : beams) {
        if (selectedIds.count(beam->id))
            return beam.get();
    }
    return nullptr;
}

std::vector<Annotation*> Scene::getSelectedAnnotations() const {
    std::vector<Annotation*> result;
    for (const auto& ann : annotations) {
        if (selectedIds.count(ann->id))
            result.push_back(ann.get());
    }
    return result;
}

Annotation* Scene::getSelectedAnnotation() const {
    for (const auto& ann : annotations) {
        if (selectedIds.count(ann->id))
            return ann.get();
    }
    return nullptr;
}

void Scene::selectAll() {
    for (const auto& elem : elements)
        selectedIds.insert(elem->id);
    for (const auto& beam : beams)
        selectedIds.insert(beam->id);
    for (const auto& ann : annotations)
        selectedIds.insert(ann->id);
}

} // namespace opticsketch
