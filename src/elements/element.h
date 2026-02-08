#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

namespace opticsketch {

enum class ElementType {
    Laser,
    Mirror,
    Lens,
    BeamSplitter,
    Detector
};

struct Transform {
    glm::vec3 position{0.0f};
    glm::quat rotation{glm::identity<glm::quat>()};
    glm::vec3 scale{1.0f};
    
    glm::mat4 getMatrix() const {
        glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 R = glm::mat4_cast(rotation);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
        return T * R * S;
    }
};

class Element {
public:
    Element(ElementType t, const std::string& elementId);
    virtual ~Element() = default;
    
    // Identification
    std::string id;
    std::string label;
    ElementType type;
    
    // Transform
    Transform transform;
    
    // State
    bool locked = false;
    bool visible = true;
    int layer = 0;
    
    // Bounds (for selection and rendering)
    glm::vec3 boundsMin{-1.0f};
    glm::vec3 boundsMax{1.0f};
    
    // Get world-space bounds (transform.getMatrix() * local bounds corners)
    void getWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const;
    
    // Transform pivot in world space = bbox center. Gizmo is drawn here; manipulator edits only transform, this is derived.
    glm::vec3 getWorldBoundsCenter() const;
    
    // Local-space center of bounds (for deriving position from desired world pivot)
    glm::vec3 getLocalBoundsCenter() const {
        return (boundsMin + boundsMax) * 0.5f;
    }
};

} // namespace opticsketch
