#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include "camera/camera.h"
#include "render/shader.h"
#include "render/gizmo.h"
#include "style/scene_style.h"

namespace opticsketch {

// Forward declarations
class Element;
class Scene;
class Beam;

struct CachedMesh {
    GLuint vao = 0, vbo = 0;
    GLsizei vertexCount = 0;
};

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
    
    // Render beams (selection state read from scene)
    void renderBeams(Scene* scene);
    
    // Render a single beam (for preview)
    void renderBeam(const Beam& beam);

    // Render Gaussian beam envelopes (semi-transparent triangle strips)
    void renderGaussianBeams(Scene* scene);

    // Render focal point markers (X-shaped) for lens elements
    void renderFocalPoints(Scene* scene);
    
    // Render gizmo for selected element; hoveredHandle: 0=X, 1=Y, 2=Z, -1=none (highlights that axis).
    // exclusiveHandle: when >= 0 (e.g. while dragging), only that axis is drawn.
    void renderGizmo(Scene* scene, GizmoType gizmoType, int hoveredHandle = -1, int exclusiveHandle = -1);

    // Render gizmo at an arbitrary world-space center (for multi-select centroid, etc.)
    void renderGizmoAt(const glm::vec3& center, GizmoType gizmoType, int hoveredHandle = -1, int exclusiveHandle = -1);
    
    // Gizmo picking: returns 0=X, 1=Y, 2=Z, -1=none. viewportX/Y relative to viewport.
    int getGizmoHoveredHandle(Element* selectedElement, GizmoType gizmoType,
                              float viewportX, float viewportY) const;
    
    // Get texture ID for ImGui display
    GLuint getTextureId() const { return textureId; }
    
    // Style
    void setStyle(SceneStyle* s) { style = s; }
    SceneStyle* getStyle() const { return style; }

    // Get camera reference
    Camera& getCamera() { return camera; }
    const Camera& getCamera() const { return camera; }
    
    // Get viewport size
    int getWidth() const { return width; }
    int getHeight() const { return height; }
    
    // Export viewport content to PNG (scene only, no grid, no gizmo). Returns true on success.
    bool exportToPng(const std::string& path, Scene* scene);

    // Export viewport content to JPEG. quality: 1-100. Returns true on success.
    bool exportToJpg(const std::string& path, Scene* scene, int quality = 90);

    // Export viewport content to a single-page PDF (JPEG-compressed). Returns true on success.
    bool exportToPdf(const std::string& path, Scene* scene);
    
    // Explicit cleanup method (call before destroying OpenGL context)
    void cleanup();
    
private:
    int width = 800;
    int height = 600;
    
    GLuint framebufferId = 0;
    GLuint textureId = 0;
    GLuint renderbufferId = 0;
    
    Camera camera;
    SceneStyle* style = nullptr;
    Shader gridShader;
    Shader materialShader;
    Gizmo* gizmo = nullptr;
    
    // Grid rendering
    GLuint gridVAO = 0;
    GLuint gridVBO = 0;
    bool gridInitialized = false;

    // Cached geometry for built-in element types (indexed by (int)ElementType)
    static constexpr int kMaxPrototypes = 16;
    CachedMesh prototypeGeometry[kMaxPrototypes];
    CachedMesh prototypeWireframe[kMaxPrototypes];
    bool prototypesInitialized = false;

    // Per-instance mesh cache for ImportedMesh elements (keyed by element ID)
    std::unordered_map<std::string, CachedMesh> meshCache;

    // Reusable buffer for beam rendering
    CachedMesh beamBuffer;
    CachedMesh gaussianBuffer;

    // Gradient background
    Shader gradientShader;

    // HDRI environment map
    GLuint hdriTexture = 0;
    std::string loadedHdriPath;
    void loadHdriTexture(const std::string& path);
    void destroyHdriTexture();

    // Bloom (Presentation mode)
    GLuint bloomFBO[2] = {0, 0};
    GLuint bloomTexture[2] = {0, 0};
    Shader bloomExtractShader;
    Shader bloomBlurShader;
    Shader bloomCompositeShader;
    GLuint fullscreenVAO = 0;
    GLuint fullscreenVBO = 0;
    bool bloomInitialized = false;

    void createFramebuffer();
    void destroyFramebuffer();
    void initGrid();
    void initPrototypeGeometry();
    void initFullscreenQuad();
    void initBloom();
    void destroyBloom();

    // Thumbnail rendering for library panel
    static constexpr int kThumbnailSize = 128;
    GLuint thumbnailFBO = 0;
    GLuint thumbnailDepthRBO = 0;
    GLuint thumbnailTextures[kMaxPrototypes] = {};
    bool thumbnailsGenerated = false;
    void destroyThumbnails();

public:
    // Render bloom post-process pass (call after endFrame in Presentation mode)
    void renderBloomPass();

    // Generate 3D thumbnail previews for all built-in element types
    void generateThumbnails();
    GLuint getThumbnailTexture(int typeIndex) const;
    bool hasThumbnails() const { return thumbnailsGenerated; }
};

} // namespace opticsketch
