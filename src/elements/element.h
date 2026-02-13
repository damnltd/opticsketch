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

enum class OpticalType { Source, Mirror, Lens, Splitter, Absorber, Prism, Grating, Passive,
                         Filter, Aperture, FiberCoupler };

struct OpticalProperties {
    OpticalType opticalType = OpticalType::Passive;
    float ior = 1.5f;               // index of refraction
    float reflectivity = 0.0f;      // 0..1
    float transmissivity = 1.0f;    // 0..1
    float focalLength = 50.0f;      // mm
    float curvatureR1 = 100.0f;     // mm (front surface)
    float curvatureR2 = -100.0f;    // mm (back surface)
    float apertureDiameter = 0.5f;  // fraction of bounds height (0..1) for aperture opening
    float gratingLineDensity = 600.0f; // lines/mm for diffraction grating
    glm::vec3 filterColor{1.0f};    // color multiplier for filter transmission
    float cauchyB = 0.0f;           // Cauchy dispersion B coefficient (m^2). 0 = no dispersion.
    // Source properties (only used when opticalType == Source)
    int sourceRayCount = 1;         // number of rays per source (1 = single ray)
    float sourceBeamWidth = 0.0f;   // collimated beam width in mm (0 = single ray)
    bool sourceIsWhiteLight = false; // emit multiple wavelengths for dispersion demos
};

struct MaterialProperties {
    float metallic = 0.0f;
    float roughness = 0.5f;
    float transparency = 0.0f;
    float fresnelIOR = 1.5f;
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

    // Optical properties
    OpticalProperties optics;

    // Material properties (for Presentation mode rendering)
    MaterialProperties material;

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
