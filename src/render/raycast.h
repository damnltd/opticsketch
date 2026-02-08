#pragma once

#include <glm/glm.hpp>
#include "camera/camera.h"
#include "elements/element.h"

namespace opticsketch {

// Forward declaration
class Camera;

// Ray casting utilities for object selection
class Raycast {
public:
    struct Ray {
        glm::vec3 origin;
        glm::vec3 direction;
    };
    
    // Generate ray from screen coordinates
    static Ray screenToRay(const Camera& camera, float screenX, float screenY, 
                          int viewportWidth, int viewportHeight);
    
    // Test ray against axis-aligned bounding box
    static bool intersectAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max, 
                             float& t);
    
    // Test ray against element bounds
    static bool intersectElement(const Ray& ray, const Element* element, 
                                 glm::mat4 transform, float& t);
    
    // Ray vs plane (plane: point on plane, normal)
    static bool intersectPlane(const Ray& ray, const glm::vec3& planePos, const glm::vec3& planeNormal,
                              float& t);
    
    // Closest point on ray to line segment; returns t (ray param), and squared distance. hit = (t >= 0 and closest point on segment).
    static float rayToSegmentSqDist(const Ray& ray, const glm::vec3& segA, const glm::vec3& segB,
                                   float& outTRay, float& outTSeg);
};

} // namespace opticsketch
