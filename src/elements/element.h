#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>
#include <vector>

namespace opticsketch {

enum class ElementType {
    Laser,
    Mirror,
    Lens,
    BeamSplitter,
    Detector,
    Filter,
    Aperture,
    Prism,
    PrismRA,
    Grating,
    FiberCoupler,
    Screen,
    Mount,
    ImportedMesh
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
    bool showLabel = true;
    int layer = 0;
    
    // Bounds (for selection and rendering)
    glm::vec3 boundsMin{-1.0f};
    glm::vec3 boundsMax{1.0f};

    // Mesh data (for ImportedMesh type only)
    std::vector<float> meshVertices;    // 6 floats per vertex (pos + normal)
    std::string meshSourcePath;         // original OBJ path for re-import
    
    // Get world-space bounds (transform.getMatrix() * local bounds corners)
    void getWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const;
    
    // Transform pivot in world space = bbox center. Gizmo is drawn here; manipulator edits only transform, this is derived.
    glm::vec3 getWorldBoundsCenter() const;
    
    // Deep-copy all fields into a new Element (new auto-generated ID, same label)
    std::unique_ptr<Element> clone() const;

    // Local-space center of bounds (for deriving position from desired world pivot)
    glm::vec3 getLocalBoundsCenter() const {
        return (boundsMin + boundsMax) * 0.5f;
    }
};

} // namespace opticsketch
