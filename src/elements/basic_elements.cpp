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
        case ElementType::ImportedMesh: prefix = "mesh"; break;
    }
    std::ostringstream oss;
    oss << prefix << "_" << (++counter);
    return oss.str();
}

std::unique_ptr<Element> createLaser(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Laser, id.empty() ? generateId(ElementType::Laser) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.5f, -2.0f);
    elem->boundsMax = glm::vec3(0.5f, 0.5f, 2.0f);
    return elem;
}

std::unique_ptr<Element> createMirror(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Mirror, id.empty() ? generateId(ElementType::Mirror) : id);
    elem->boundsMin = glm::vec3(-1.0f, -1.0f, -0.1f);
    elem->boundsMax = glm::vec3(1.0f, 1.0f, 0.1f);
    return elem;
}

std::unique_ptr<Element> createLens(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Lens, id.empty() ? generateId(ElementType::Lens) : id);
    elem->boundsMin = glm::vec3(-1.27f, -1.27f, -0.5f); // 25.4mm diameter, 5mm thick
    elem->boundsMax = glm::vec3(1.27f, 1.27f, 0.5f);
    return elem;
}

std::unique_ptr<Element> createBeamSplitter(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::BeamSplitter, id.empty() ? generateId(ElementType::BeamSplitter) : id);
    elem->boundsMin = glm::vec3(-1.0f, -1.0f, -1.0f);
    elem->boundsMax = glm::vec3(1.0f, 1.0f, 1.0f);
    return elem;
}

std::unique_ptr<Element> createDetector(const std::string& id) {
    auto elem = std::make_unique<Element>(ElementType::Detector, id.empty() ? generateId(ElementType::Detector) : id);
    elem->boundsMin = glm::vec3(-0.5f, -0.5f, -0.1f);
    elem->boundsMax = glm::vec3(0.5f, 0.5f, 0.1f);
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
        default: return nullptr;
    }
}

} // namespace opticsketch
