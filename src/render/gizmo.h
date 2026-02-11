#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include "camera/camera.h"
#include "elements/element.h"

namespace opticsketch {

// Forward declarations
class Shader;

enum class GizmoType {
    Move,
    Rotate,
    Scale
};

class Gizmo {
public:
    Gizmo();
    
    // Initialize gizmo (load shader)
    void init(Shader* shader);

    // Cleanup GPU resources
    void cleanup();
    
    // Render gizmo for selected element; hoveredHandle: 0=X, 1=Y, 2=Z, -1=none (that axis highlighted).
    // exclusiveHandle: when >= 0 (e.g. while dragging), only that axis is drawn (and highlighted).
    void render(const Camera& camera, const Element* element, GizmoType type, int viewportWidth, int viewportHeight, int hoveredHandle = -1, int exclusiveHandle = -1);
    
    // Check if gizmo handle is under mouse. Returns 0=X, 1=Y, 2=Z, -1=none.
    // viewportX/Y are relative to viewport (e.g. mouse - imageMin).
    int getHoveredHandle(const Camera& camera, const Element* element, GizmoType type,
                        float viewportX, float viewportY, int viewportWidth, int viewportHeight);
    
    // Get axis direction and length for the given handle (for move/scale). Axis is from element position.
    static void getMoveAxis(int handle, glm::vec3& outAxisDir, float& outLength);
    static void getRotateAxis(int handle, glm::vec3& outAxisDir, float& outRadius);
    
private:
    Shader* gizmoShader = nullptr;
    GLuint solidVAO = 0, solidVBO = 0;
    
    // Gizmo rendering helpers. When exclusiveHandle >= 0, only that axis is drawn (and highlighted).
    void renderMoveGizmo(const Camera& camera, const glm::vec3& position, const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle = -1);
    void renderRotateGizmo(const Camera& camera, const glm::vec3& position, const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle = -1);
    void renderScaleGizmo(const Camera& camera, const glm::vec3& position, const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle = -1);
    
    // Thick lines as quads (glLineWidth > 1 is ignored in OpenGL core profile)
    void renderSolid(const std::vector<float>& vertices);
    void makeThickLineQuad(const glm::vec3& camPos, const glm::vec3& a, const glm::vec3& b, float halfWidth, std::vector<float>& outVertices);
    std::vector<float> generateCircleLineLoop(const glm::vec3& center, const glm::vec3& normal, float radius, int segments);
};

} // namespace opticsketch
