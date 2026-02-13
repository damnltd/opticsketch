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

// Gizmo handle IDs (int for backward compat with existing code)
namespace GizmoHandle {
    constexpr int None   = -1;
    constexpr int X      = 0;   // X axis
    constexpr int Y      = 1;   // Y axis
    constexpr int Z      = 2;   // Z axis
    constexpr int XY     = 3;   // XY plane (Move only)
    constexpr int YZ     = 4;   // YZ plane (Move only)
    constexpr int XZ     = 5;   // XZ plane (Move only)
    constexpr int Center = 6;   // Center handle (Move = free drag, Scale = uniform)
}

enum class GizmoSpace { World, Local };

class Gizmo {
public:
    Gizmo();
    
    // Initialize gizmo (load shader)
    void init(Shader* shader);

    // Cleanup GPU resources
    void cleanup();
    
    // Render gizmo for selected element; hoveredHandle: axis/plane/center handle ID, -1=none.
    // exclusiveHandle: when >= 0 (e.g. while dragging), only that handle is drawn (and highlighted).
    // orientation: gizmo axis orientation (identity = world space, element rotation = local space).
    // dragAngle/dragStartAngle: for rotation arc feedback during drag (radians).
    void render(const Camera& camera, const Element* element, GizmoType type,
                int viewportWidth, int viewportHeight, int hoveredHandle = -1, int exclusiveHandle = -1,
                const glm::mat3& orientation = glm::mat3(1.0f),
                float dragAngle = 0.0f, float dragStartAngle = 0.0f);

    // Check if gizmo handle is under mouse. Returns handle ID or -1.
    // viewportX/Y are relative to viewport (e.g. mouse - imageMin).
    int getHoveredHandle(const Camera& camera, const Element* element, GizmoType type,
                        float viewportX, float viewportY, int viewportWidth, int viewportHeight,
                        const glm::mat3& orientation = glm::mat3(1.0f));
    
    // Get axis direction and length for the given handle (for move/scale). Axis is from element position.
    static void getMoveAxis(int handle, glm::vec3& outAxisDir, float& outLength);
    static void getRotateAxis(int handle, glm::vec3& outAxisDir, float& outRadius);
    
private:
    Shader* gizmoShader = nullptr;
    GLuint solidVAO = 0, solidVBO = 0;
    
    // Gizmo rendering helpers
    void renderMoveGizmo(const Camera& camera, const glm::vec3& position, const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle, const glm::mat3& orientation);
    void renderRotateGizmo(const Camera& camera, const glm::vec3& position, const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle, const glm::mat3& orientation, float dragAngle, float dragStartAngle);
    void renderScaleGizmo(const Camera& camera, const glm::vec3& position, const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle, const glm::mat3& orientation);
    
    // Thick lines as quads (glLineWidth > 1 is ignored in OpenGL core profile)
    void renderSolid(const std::vector<float>& vertices);
    void makeThickLineQuad(const glm::vec3& camPos, const glm::vec3& a, const glm::vec3& b, float halfWidth, std::vector<float>& outVertices);
    std::vector<float> generateCircleLineLoop(const glm::vec3& center, const glm::vec3& normal, float radius, int segments);

    // Solid geometry helpers for Maya/Blender-style gizmo tips
    void makeConeTip(const glm::vec3& tipPos, const glm::vec3& axisDir, float coneLength, float coneRadius, int segments, std::vector<float>& outVertices);
    void makeCubeTip(const glm::vec3& center, float halfSize, std::vector<float>& outVertices);
    void makePlaneSquare(const glm::vec3& position, const glm::vec3& axis1, const glm::vec3& axis2, float offset, float size, const glm::vec3& normal, std::vector<float>& outVertices);
    void makeArc(const glm::vec3& camPos, const glm::vec3& center, const glm::vec3& axisDir, float startAngle, float sweepAngle, float radius, float halfWidth, int segments, std::vector<float>& outVertices);
};

} // namespace opticsketch
