#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace opticsketch {

struct MeshData {
    std::vector<float> vertices;    // 6 floats per vertex: pos(3) + normal(3)
    glm::vec3 boundsMin{0.0f};
    glm::vec3 boundsMax{0.0f};
};

// Load an OBJ file into MeshData. Centers mesh at origin and normalizes to fit
// within a unit bounding box. Returns true on success.
bool loadObjFile(const std::string& path, MeshData& outData);

} // namespace opticsketch
