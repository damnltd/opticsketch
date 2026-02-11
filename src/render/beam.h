#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace opticsketch {

// Represents a single beam segment (3D line)
class Beam {
public:
    Beam(const std::string& beamId = "");
    
    std::string id;
    std::string label;
    
    // Beam segment endpoints
    glm::vec3 start;
    glm::vec3 end;
    
    // Visual properties
    glm::vec3 color{1.0f, 0.0f, 0.0f};  // Default red for laser beams
    float width = 2.0f;                  // Line width in pixels
    bool visible = true;
    int layer = 0;
    
    // Get beam direction (normalized)
    glm::vec3 getDirection() const;
    
    // Get beam length
    float getLength() const;
    
    // Generate unique ID
    static std::string generateId();
};

} // namespace opticsketch
