#include "elements/basic_elements.h"
#include "render/mesh_loader.h"
#include <sstream>
#include <random>

namespace opticsketch {

// Simple ID generator
static std::string generateId(ElementType type) {
    static int counter = 0;
    std::string prefix;
    switch (type) {
        case ElementType::Laser: prefix = "laser"; break;
        case ElementType::Mirror: prefix = "mirror"; break;
        case ElementType::Lens: prefix = "lens"; break;
        case ElementType::BeamSplitter: prefix = "bs"; break;
        case ElementType::Detector: prefix = "detector"; break;
        case ElementType::Filter: prefix = "filter"; break;
        case ElementType::Aperture: prefix = "aperture"; break;
        case ElementType::Prism: prefix = "prism"; break;
        case ElementType::PrismRA: prefix = "prismra"; break;
        case ElementType::Grating: prefix = "grating"; break;
        case ElementType::FiberCoupler: prefix = "fiber"; break;
        case ElementType::Screen: prefix = "screen"; break;
        case ElementType::Mount: prefix = "mount"; break;
        case ElementType::ImportedMesh: prefix = "mesh"; break;
    }
    std::ostringstream oss;
    oss << prefix << "_" << (++counter);
    return oss.str();
}

std::unique_ptr<Element> createLaser(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Laser, id.empty() ? generateId(ElementType::Laser) : id);
    elem->boundsMin = glm::vec3(-0.25f, -0.85f, -0.25f);
    elem->boundsMax = glm::vec3(0.25f, 0.85f, 0.25f);
    elem->optics.opticalType = OpticalType::Source;
    elem->optics.reflectivity = 0.0f;
    elem->optics.transmissivity = 0.0f;
    elem->material.metallic = 0.3f;
    elem->material.roughness = 0.4f;
    return elem;
}

std::unique_ptr<Element> createMirror(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Mirror, id.empty() ? generateId(ElementType::Mirror) : id);
    elem->boundsMin = glm::vec3(-0.55f, -0.55f, -0.06f);
    elem->boundsMax = glm::vec3(0.55f, 0.55f, 0.06f);
    elem->optics.opticalType = OpticalType::Mirror;
    elem->optics.reflectivity = 1.0f;
    elem->optics.transmissivity = 0.0f;
    elem->material.metallic = 0.95f;
    elem->material.roughness = 0.05f;
    return elem;
}

std::unique_ptr<Element> createLens(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Lens, id.empty() ? generateId(ElementType::Lens) : id);
    elem->boundsMin = glm::vec3(-0.55f, -0.55f, -0.12f);
    elem->boundsMax = glm::vec3(0.55f, 0.55f, 0.12f);
    elem->optics.opticalType = OpticalType::Lens;
    elem->optics.ior = 1.5168f;  // BK7
    elem->optics.focalLength = 50.0f;
    elem->optics.curvatureR1 = 100.0f;
    elem->optics.curvatureR2 = -100.0f;
    elem->material.transparency = 0.7f;
    elem->material.fresnelIOR = 1.5168f;
    return elem;
}

std::unique_ptr<Element> createBeamSplitter(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::BeamSplitter, id.empty() ? generateId(ElementType::BeamSplitter) : id);
    elem->boundsMin = glm::vec3(-0.45f, -0.45f, -0.45f);
    elem->boundsMax = glm::vec3(0.45f, 0.45f, 0.45f);
    elem->optics.opticalType = OpticalType::Splitter;
    elem->optics.reflectivity = 0.5f;
    elem->optics.transmissivity = 0.5f;
    elem->material.transparency = 0.4f;
    return elem;
}

std::unique_ptr<Element> createDetector(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Detector, id.empty() ? generateId(ElementType::Detector) : id);
    elem->boundsMin = glm::vec3(-0.4f, -0.4f, -0.15f);
    elem->boundsMax = glm::vec3(0.4f, 0.4f, 0.15f);
    elem->optics.opticalType = OpticalType::Absorber;
    elem->optics.reflectivity = 0.0f;
    elem->optics.transmissivity = 0.0f;
    return elem;
}

std::unique_ptr<Element> createFilter(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Filter, id.empty() ? generateId(ElementType::Filter) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.5f, -0.025f);
    elem->boundsMax = glm::vec3(0.5f, 0.5f, 0.025f);
    elem->optics.opticalType = OpticalType::Filter;
    elem->optics.transmissivity = 0.5f;
    elem->optics.filterColor = glm::vec3(0.6f, 0.4f, 0.8f);
    elem->material.transparency = 0.5f;
    return elem;
}

std::unique_ptr<Element> createAperture(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Aperture, id.empty() ? generateId(ElementType::Aperture) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.5f, -0.03f);
    elem->boundsMax = glm::vec3(0.5f, 0.5f, 0.03f);
    elem->optics.opticalType = OpticalType::Aperture;
    elem->optics.apertureDiameter = 0.5f;
    return elem;
}

std::unique_ptr<Element> createPrism(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Prism, id.empty() ? generateId(ElementType::Prism) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.433f, -0.5f);
    elem->boundsMax = glm::vec3(0.5f, 0.433f, 0.5f);
    elem->optics.opticalType = OpticalType::Prism;
    elem->optics.ior = 1.5168f;
    elem->material.transparency = 0.6f;
    elem->material.fresnelIOR = 1.5168f;
    return elem;
}

std::unique_ptr<Element> createPrismRA(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::PrismRA, id.empty() ? generateId(ElementType::PrismRA) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.5f, -0.5f);
    elem->boundsMax = glm::vec3(0.5f, 0.5f, 0.5f);
    elem->optics.opticalType = OpticalType::Prism;
    elem->optics.ior = 1.5168f;
    elem->material.transparency = 0.6f;
    elem->material.fresnelIOR = 1.5168f;
    return elem;
}

std::unique_ptr<Element> createGrating(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Grating, id.empty() ? generateId(ElementType::Grating) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.5f, -0.02f);
    elem->boundsMax = glm::vec3(0.5f, 0.5f, 0.02f);
    elem->optics.opticalType = OpticalType::Grating;
    elem->optics.gratingLineDensity = 600.0f;
    return elem;
}

std::unique_ptr<Element> createFiberCoupler(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::FiberCoupler, id.empty() ? generateId(ElementType::FiberCoupler) : id);
    elem->boundsMin = glm::vec3(-0.2f, -0.4f, -0.2f);
    elem->boundsMax = glm::vec3(0.2f, 0.4f, 0.2f);
    elem->optics.opticalType = OpticalType::FiberCoupler;
    elem->optics.transmissivity = 0.7f;
    return elem;
}

std::unique_ptr<Element> createScreen(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Screen, id.empty() ? generateId(ElementType::Screen) : id);
    elem->boundsMin = glm::vec3(-0.75f, -1.0f, -0.05f);
    elem->boundsMax = glm::vec3(0.75f, 1.0f, 0.05f);
    elem->optics.opticalType = OpticalType::Absorber;
    elem->optics.reflectivity = 0.0f;
    elem->optics.transmissivity = 0.0f;
    return elem;
}

std::unique_ptr<Element> createMount(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Mount, id.empty() ? generateId(ElementType::Mount) : id);
    elem->boundsMin = glm::vec3(-0.3f, -0.62f, -0.3f);
    elem->boundsMax = glm::vec3(0.3f, 0.55f, 0.3f);
    elem->optics.opticalType = OpticalType::Passive;
    elem->material.metallic = 0.8f;
    elem->material.roughness = 0.3f;
    return elem;
}

std::unique_ptr<Element> createMeshElement(const std::string& objPath, const std::string& id) {
    MeshData data;
    if (!loadObjFile(objPath, data)) return nullptr;

    auto elem = std::make_unique<Element>(ElementType::ImportedMesh,
                                          id.empty() ? generateId(ElementType::ImportedMesh) : id);
    elem->meshVertices = std::move(data.vertices);
    elem->meshSourcePath = objPath;
    elem->boundsMin = data.boundsMin;
    elem->boundsMax = data.boundsMax;

    // Derive label from filename
    std::string name = objPath;
    auto pos = name.find_last_of("/\\");
    if (pos != std::string::npos) name = name.substr(pos + 1);
    auto dot = name.rfind('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    elem->label = name;

    return elem;
}

std::unique_ptr<Element> createElement(ElementType type, const std::string& id) {
    switch (type) {
        case ElementType::Laser: return createLaser(id);
        case ElementType::Mirror: return createMirror(id);
        case ElementType::Lens: return createLens(id);
        case ElementType::BeamSplitter: return createBeamSplitter(id);
        case ElementType::Detector: return createDetector(id);
        case ElementType::Filter: return createFilter(id);
        case ElementType::Aperture: return createAperture(id);
        case ElementType::Prism: return createPrism(id);
        case ElementType::PrismRA: return createPrismRA(id);
        case ElementType::Grating: return createGrating(id);
        case ElementType::FiberCoupler: return createFiberCoupler(id);
        case ElementType::Screen: return createScreen(id);
        case ElementType::Mount: return createMount(id);
        default: return nullptr;
    }
}

} // namespace opticsketch
