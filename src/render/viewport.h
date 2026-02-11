#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include "camera/camera.h"
#include "render/shader.h"
#include "render/gizmo.h"

namespace opticsketch {

// Forward declarations
class Element;
class Scene;
class Beam;

class Viewport {
public:
    Viewport();
    ~Viewport();
    
    // Initialize viewport with size
    void init(int width, int height);
    
    // Resize viewport
    void resize(int width, int height);
    
    // Begin rendering to viewport
    void beginFrame();
    
    // End rendering and return texture ID for ImGui
    void endFrame();
    
    // Render grid
    void renderGrid(float spacing = 25.0f, int gridSize = 100);
    
    // Render scene elements. If forExport is true, no selection highlight or wireframe (for PNG export).
    void renderScene(Scene* scene, bool forExport = false);
    
    // Render beams (selectedBeam is highlighted)
    void renderBeams(Scene* scene, const Beam* selectedBeam = nullptr);
    
    // Render a single beam (for preview)
    void renderBeam(const Beam& beam);
    
    // Render gizmo for selected element; hoveredHandle: 0=X, 1=Y, 2=Z, -1=none (highlights that axis).
    // exclusiveHandle: when >= 0 (e.g. while dragging), only that axis is drawn.
    void renderGizmo(Scene* scene, GizmoType gizmoType, int hoveredHandle = -1, int exclusiveHandle = -1);
    
    // Gizmo picking: returns 0=X, 1=Y, 2=Z, -1=none. viewportX/Y relative to viewport.
    int getGizmoHoveredHandle(Element* selectedElement, GizmoType gizmoType,
                              float viewportX, float viewportY) const;
    
    // Get texture ID for ImGui display
    GLuint getTextureId() const { return textureId; }
    
    // Get camera reference
    Camera& getCamera() { return camera; }
    const Camera& getCamera() const { return camera; }
    
    // Get viewport size
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
    // Export viewport content to PNG (scene only, no grid, no gizmo). Returns true on success.
    bool exportToPng(const std::string& path, Scene* scene);
    
    // Explicit cleanup method (call before destroying OpenGL context)
    void cleanup();
    
private:
    int width = 800;
    int height = 600;
    
    GLuint framebufferId = 0;
    GLuint textureId = 0;
    GLuint renderbufferId = 0;
    
    Camera camera;
    Shader gridShader;
    Gizmo* gizmo = nullptr;
    
    // Grid rendering
    GLuint gridVAO = 0;
    GLuint gridVBO = 0;
    bool gridInitialized = false;
    
    void createFramebuffer();
    void destroyFramebuffer();
    void initGrid();
};

} // namespace opticsketch
