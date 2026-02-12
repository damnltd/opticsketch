#include "elements/element.h"
#include <algorithm>

namespace opticsketch {

Element::Element(ElementType t, const std::string& elementId)
    : type(t), id(elementId) {
    // Generate default label from type
    switch (type) {
        case ElementType::Laser: label = "Laser"; break;
        case ElementType::Mirror: label = "Mirror"; break;
        case ElementType::Lens: label = "Lens"; break;
        case ElementType::BeamSplitter: label = "Beam Splitter"; break;
        case ElementType::Detector: label = "Detector"; break;
        case ElementType::Filter: label = "Filter"; break;
        case ElementType::Aperture: label = "Aperture"; break;
        case ElementType::Prism: label = "Prism"; break;
        case ElementType::PrismRA: label = "Prism RA"; break;
        case ElementType::Grating: label = "Grating"; break;
        case ElementType::FiberCoupler: label = "Fiber Coupler"; break;
        case ElementType::Screen: label = "Screen"; break;
        case ElementType::Mount: label = "Mount"; break;
        case ElementType::ImportedMesh: break;
    }
}

void Element::getWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const {
    glm::mat4 matrix = transform.getMatrix();
    
    // Transform all 8 corners of the bounding box
    glm::vec3 corners[8] = {
        glm::vec3(boundsMin.x, boundsMin.y, boundsMin.z),
        glm::vec3(boundsMax.x, boundsMin.y, boundsMin.z),
        glm::vec3(boundsMin.x, boundsMax.y, boundsMin.z),
        glm::vec3(boundsMax.x, boundsMax.y, boundsMin.z),
        glm::vec3(boundsMin.x, boundsMin.y, boundsMax.z),
        glm::vec3(boundsMax.x, boundsMin.y, boundsMax.z),
        glm::vec3(boundsMin.x, boundsMax.y, boundsMax.z),
        glm::vec3(boundsMax.x, boundsMax.y, boundsMax.z)
    };
    
    outMin = glm::vec3(FLT_MAX);
    outMax = glm::vec3(-FLT_MAX);
    
    for (int i = 0; i < 8; ++i) {
        glm::vec3 worldPos = glm::vec3(matrix * glm::vec4(corners[i], 1.0f));
        outMin = glm::min(outMin, worldPos);
        outMax = glm::max(outMax, worldPos);
    }
}

// Pivot = bbox center in world. Equals position + R*(S*localCenter) by construction (getWorldBounds uses transform.getMatrix()).
glm::vec3 Element::getWorldBoundsCenter() const {
    glm::vec3 outMin, outMax;
    getWorldBounds(outMin, outMax);
    return (outMin + outMax) * 0.5f;
}

std::unique_ptr<Element> Element::clone() const {
    auto e = std::make_unique<Element>(type, ""); // empty ID — Scene::addElement will ensure uniqueness
    e->label = label;
    e->transform = transform;
    e->locked = locked;
    e->visible = visible;
    e->showLabel = showLabel;
    e->layer = layer;
    e->boundsMin = boundsMin;
    e->boundsMax = boundsMax;
    e->optics = optics;
    e->material = material;
    e->meshVertices = meshVertices;
    e->meshSourcePath = meshSourcePath;
    return e;
}

} // namespace opticsketch
