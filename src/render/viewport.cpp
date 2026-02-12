#include "render/viewport.h"
#include "render/gizmo.h"
#include "render/beam.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "export/export_png.h"
#include "stb_image.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <cfloat>
#include <limits>
#include <tuple>
#include <glm/gtc/matrix_inverse.hpp>

namespace opticsketch {

// Forward declarations for helper functions defined later in this file
static CachedMesh createCachedMesh(const std::vector<float>& vertices, int floatsPerVertex = 6);
static void deleteCachedMesh(CachedMesh& mesh);

Viewport::Viewport() {
    camera.setAspectRatio(static_cast<float>(width) / height);
}

Viewport::~Viewport() {
    // Destructor - cleanup will be called explicitly before context is destroyed
    // Don't do anything here to avoid double cleanup
}

void Viewport::cleanup() {
    // Cleanup OpenGL resources
    // This should be called explicitly before destroying the OpenGL context
    destroyFramebuffer();
    if (gridVAO != 0) {
        glDeleteVertexArrays(1, &gridVAO);
        glDeleteBuffers(1, &gridVBO);
        gridVAO = 0;
        gridVBO = 0;
    }
    // Delete prototype geometry caches
    for (int i = 0; i < kMaxPrototypes; i++) {
        deleteCachedMesh(prototypeGeometry[i]);
        deleteCachedMesh(prototypeWireframe[i]);
    }
    prototypesInitialized = false;
    // Delete per-instance mesh caches
    for (auto& [id, mesh] : meshCache) {
        deleteCachedMesh(mesh);
    }
    meshCache.clear();
    // Delete beam buffer
    deleteCachedMesh(beamBuffer);
    deleteCachedMesh(gaussianBuffer);
    // HDRI cleanup
    destroyHdriTexture();
    // Thumbnail cleanup
    destroyThumbnails();
    // Bloom cleanup
    destroyBloom();
    if (gizmo) {
        gizmo->cleanup();
        delete gizmo;
        gizmo = nullptr;
    }
}

void Viewport::init(int w, int h) {
    width = w;
    height = h;
    camera.setAspectRatio(static_cast<float>(width) / height);
    createFramebuffer();
    
    // Initialize gizmo
    gizmo = new Gizmo();
    gizmo->init(&gridShader);
    
    // Try to load grid shader from files (relative to executable or source dir)
    // First try relative to current directory, then try relative to source
    bool shaderLoaded = false;
    const char* shaderPaths[] = {
        "assets/shaders/grid.vert",
        "../assets/shaders/grid.vert",
        "../../assets/shaders/grid.vert"
    };
    
    for (const char* vertPath : shaderPaths) {
        std::string fragPath = vertPath;
        fragPath.replace(fragPath.find(".vert"), 5, ".frag");
        
        if (gridShader.loadFromFiles(vertPath, fragPath)) {
            shaderLoaded = true;
            break;
        }
    }
    
    if (!shaderLoaded) {
        std::cerr << "Failed to load grid shader from files, using embedded source" << std::endl;
        // Fallback: use embedded shader source
        const char* vertSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 uModel = mat4(1.0);
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
out vec3 FragPos;
out vec3 Normal;
void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = uNormalMatrix * aNormal;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
)";
        const char* fragSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragPos;
in vec3 Normal;
uniform vec3 uColor;
uniform float uAlpha;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform float uAmbientStrength = 0.14;
uniform float uSpecularStrength = 0.55;
uniform float uShininess = 48.0;
uniform float uEmissive = 0.0;
void main() {
    if (uEmissive > 0.5) { FragColor = vec4(uColor, uAlpha); return; }
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    vec3 fillDir = normalize(-uLightPos - FragPos);
    float fillDiff = max(dot(norm, fillDir), 0.0);
    vec3 fillLight = fillDiff * vec3(0.4, 0.45, 0.5);
    vec3 ambient = uAmbientStrength * vec3(1.0, 1.0, 1.0);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), uShininess);
    vec3 specular = uSpecularStrength * spec * vec3(1.0, 1.0, 1.0);
    vec3 result = (ambient + diffuse + fillLight + specular) * uColor;
    result = result / (1.0 + 0.15 * length(result));
    FragColor = vec4(result, uAlpha);
}
)";
        gridShader.loadFromSource(vertSource, fragSource);
    }
    
    // Load material shader (uses same vertex shader as grid)
    {
        bool matShaderLoaded = false;
        const char* matShaderPaths[] = {
            "assets/shaders/material.frag",
            "../assets/shaders/material.frag",
            "../../assets/shaders/material.frag"
        };
        for (const char* fragPath : matShaderPaths) {
            // Derive vert path from the same directory
            std::string fp(fragPath);
            std::string dir = fp.substr(0, fp.rfind('/'));
            std::string vertPath = dir + "/grid.vert";
            if (materialShader.loadFromFiles(vertPath.c_str(), fragPath)) {
                matShaderLoaded = true;
                break;
            }
        }
        if (!matShaderLoaded) {
            // Fallback: use embedded material shader source
            const char* matVertSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
uniform mat4 uModel = mat4(1.0);
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat3 uNormalMatrix;
out vec3 FragPos;
out vec3 Normal;
void main() {
    FragPos = vec3(uModel * vec4(aPos, 1.0));
    Normal = uNormalMatrix * aNormal;
    gl_Position = uProjection * uView * vec4(FragPos, 1.0);
}
)";
            const char* matFragSource = R"(
#version 330 core
out vec4 FragColor;
in vec3 FragPos;
in vec3 Normal;
uniform vec3 uColor;
uniform float uAlpha;
uniform vec3 uLightPos;
uniform vec3 uViewPos;
uniform float uAmbientStrength = 0.14;
uniform float uSpecularStrength = 0.55;
uniform float uShininess = 48.0;
uniform float uMetallic = 0.0;
uniform float uRoughness = 0.5;
uniform float uTransparency = 0.0;
uniform float uFresnelIOR = 1.5;
uniform sampler2D uEnvMap;
uniform bool uHasEnvMap = false;
uniform float uEnvIntensity = 1.0;
uniform float uEnvRotation = 0.0;
const float PI = 3.14159265359;
vec3 sampleEquirectangular(vec3 dir, float rotation) {
    float cosR = cos(rotation); float sinR = sin(rotation);
    vec3 rd = vec3(cosR*dir.x + sinR*dir.z, dir.y, -sinR*dir.x + cosR*dir.z);
    float u = atan(rd.z, rd.x) / (2.0*PI) + 0.5;
    float v = asin(clamp(rd.y, -1.0, 1.0)) / PI + 0.5;
    return texture(uEnvMap, vec2(u, v)).rgb;
}
void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0);
    vec3 fillDir = normalize(-uLightPos - FragPos);
    float fillDiff = max(dot(norm, fillDir), 0.0);
    vec3 fillLight = fillDiff * vec3(0.4, 0.45, 0.5);
    vec3 ambient = uAmbientStrength * vec3(1.0);
    float shininess = max(2.0, uShininess * (1.0 - uRoughness * 0.9));
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);
    float cosTheta = max(dot(viewDir, norm), 0.0);
    float f0 = (uFresnelIOR - 1.0) / (uFresnelIOR + 1.0);
    f0 = f0 * f0;
    float fresnel = f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
    vec3 specColor = mix(vec3(1.0), uColor, uMetallic);
    float specStrength = mix(uSpecularStrength, uSpecularStrength * 2.0, uMetallic);
    vec3 specular = specStrength * spec * specColor;
    vec3 baseColor = uColor * (1.0 - uMetallic * 0.7);
    vec3 result = (ambient + diffuse + fillLight) * baseColor + specular;
    if (uHasEnvMap) {
        vec3 reflectDir = reflect(-viewDir, norm);
        vec3 envColor = sampleEquirectangular(reflectDir, uEnvRotation);
        float envStrength = 1.0 - uRoughness * 0.85;
        vec3 envTint = mix(vec3(1.0), uColor, uMetallic);
        result += envColor * envTint * envStrength * fresnel * uEnvIntensity;
    }
    if (uTransparency > 0.01) { result += fresnel * vec3(0.3) * uTransparency; }
    result = result / (1.0 + 0.15 * length(result));
    float alpha = uAlpha * (1.0 - uTransparency * (1.0 - fresnel * 0.5));
    FragColor = vec4(result, alpha);
}
)";
            materialShader.loadFromSource(matVertSource, matFragSource);
        }
    }

    // Load gradient background shader
    {
        bool gradLoaded = false;
        const char* gradPaths[] = {
            "assets/shaders/gradient_bg.frag",
            "../assets/shaders/gradient_bg.frag",
            "../../assets/shaders/gradient_bg.frag"
        };
        for (const char* fragPath : gradPaths) {
            std::string fp(fragPath);
            std::string dir = fp.substr(0, fp.rfind('/'));
            std::string vertPath = dir + "/fullscreen.vert";
            if (gradientShader.loadFromFiles(vertPath.c_str(), fragPath)) {
                gradLoaded = true;
                break;
            }
        }
        if (!gradLoaded) {
            const char* gradVert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
void main() { TexCoords = aTexCoords; gl_Position = vec4(aPos, 0.0, 1.0); }
)";
            const char* gradFrag = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform vec3 uTopColor;
uniform vec3 uBottomColor;
void main() {
    vec3 color = mix(uBottomColor, uTopColor, TexCoords.y);
    FragColor = vec4(color, 1.0);
}
)";
            gradientShader.loadFromSource(gradVert, gradFrag);
        }
    }

    initGrid();
    initPrototypeGeometry();
}

void Viewport::resize(int w, int h) {
    if (width == w && height == h) return;
    
    width = w;
    height = h;
    camera.setAspectRatio(static_cast<float>(width) / height);
    
    destroyFramebuffer();
    createFramebuffer();
}

void Viewport::createFramebuffer() {
    // Create framebuffer
    glGenFramebuffers(1, &framebufferId);
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    
    // Create texture (RGBA16F for HDR bloom support in Presentation mode)
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);
    
    // Create renderbuffer for depth/stencil
    glGenRenderbuffers(1, &renderbufferId);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbufferId);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, renderbufferId);
    
    // Check framebuffer completeness
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "Framebuffer is not complete!" << std::endl;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Viewport::destroyFramebuffer() {
    // Safety check - only delete if IDs are valid
    if (framebufferId != 0) {
        glDeleteFramebuffers(1, &framebufferId);
        framebufferId = 0;
    }
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
    if (renderbufferId != 0) {
        glDeleteRenderbuffers(1, &renderbufferId);
        renderbufferId = 0;
    }
}

void Viewport::beginFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, framebufferId);
    glViewport(0, 0, width, height);

    // Clear with style background color (Schematic overrides to white)
    glm::vec3 bg = style ? style->bgColor : glm::vec3(0.05f, 0.05f, 0.06f);
    bool isSchematic = style && style->renderMode == RenderMode::Schematic;
    if (isSchematic) bg = glm::vec3(1.0f, 1.0f, 1.0f);
    glClearColor(bg.r, bg.g, bg.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Gradient background (not in Schematic mode)
    if (!isSchematic && style && style->bgMode == BackgroundMode::Gradient) {
        initFullscreenQuad();
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        gradientShader.use();
        gradientShader.setVec3("uTopColor", style->bgGradientTop);
        gradientShader.setVec3("uBottomColor", style->bgGradientBottom);
        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
    }

    // Enable depth testing and face culling for solid rendering
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
}

void Viewport::endFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
}

void Viewport::initGrid() {
    if (gridInitialized) return;
    
    // Create a large grid - we'll regenerate it dynamically in renderGrid
    // For now, create a placeholder
    std::vector<float> vertices;
    
    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glBindVertexArray(0);
    gridInitialized = true;
}

static CachedMesh createCachedMesh(const std::vector<float>& vertices, int floatsPerVertex) {
    CachedMesh mesh;
    mesh.vertexCount = static_cast<GLsizei>(vertices.size() / floatsPerVertex);
    if (mesh.vertexCount == 0) return mesh;

    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);
    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, floatsPerVertex * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, floatsPerVertex * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
    return mesh;
}

static void deleteCachedMesh(CachedMesh& mesh) {
    if (mesh.vao != 0) {
        glDeleteVertexArrays(1, &mesh.vao);
        glDeleteBuffers(1, &mesh.vbo);
        mesh.vao = 0;
        mesh.vbo = 0;
        mesh.vertexCount = 0;
    }
}

void Viewport::renderGrid(float spacing, int gridSize) {
    // Suppress grid in Schematic mode (clean white background)
    if (style && style->renderMode == RenderMode::Schematic) return;

    if (!gridInitialized) {
        initGrid();
    }
    
    // Generate grid vertices (position + normal for ground plane)
    std::vector<float> vertices;
    const float halfSize = (gridSize * spacing) / 2.0f;
    const float nx = 0.0f, ny = 1.0f, nz = 0.0f;
    
    for (int i = -gridSize / 2; i <= gridSize / 2; ++i) {
        float z = i * spacing;
        vertices.insert(vertices.end(), {-halfSize, 0.0f, z, nx, ny, nz});
        vertices.insert(vertices.end(), {halfSize,  0.0f, z, nx, ny, nz});
    }
    for (int i = -gridSize / 2; i <= gridSize / 2; ++i) {
        float x = i * spacing;
        vertices.insert(vertices.end(), {x, 0.0f, -halfSize, nx, ny, nz});
        vertices.insert(vertices.end(), {x, 0.0f, halfSize,  nx, ny, nz});
    }
    
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    
    gridShader.use();
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);
    gridShader.setVec3("uColor", style ? style->gridColor : glm::vec3(0.3f, 0.3f, 0.35f));
    gridShader.setFloat("uAlpha", style ? style->gridAlpha : 0.5f);
    
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size() / 6));
    glBindVertexArray(0);
}

// Helper functions to generate 3D shape wireframes
// Solid geometry generation functions (with normals)
static std::vector<float> generateCubeSolid(float size) {
    float s = size * 0.5f;
    // Generate cube with positions and normals (6 faces * 2 triangles * 3 vertices * 6 floats)
    std::vector<float> vertices;
    
    // Helper to add a triangle with normals
    auto addTriangle = [&](float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float nx, float ny, float nz) {
        vertices.insert(vertices.end(), {x1, y1, z1, nx, ny, nz});
        vertices.insert(vertices.end(), {x2, y2, z2, nx, ny, nz});
        vertices.insert(vertices.end(), {x3, y3, z3, nx, ny, nz});
    };
    
    // Front face (Z+)
    addTriangle(-s, -s, s, s, -s, s, s, s, s, 0, 0, 1);
    addTriangle(-s, -s, s, s, s, s, -s, s, s, 0, 0, 1);
    // Back face (Z-)
    addTriangle(s, -s, -s, -s, -s, -s, -s, s, -s, 0, 0, -1);
    addTriangle(s, -s, -s, -s, s, -s, s, s, -s, 0, 0, -1);
    // Right face (X+)
    addTriangle(s, -s, s, s, -s, -s, s, s, -s, 1, 0, 0);
    addTriangle(s, -s, s, s, s, -s, s, s, s, 1, 0, 0);
    // Left face (X-)
    addTriangle(-s, -s, -s, -s, -s, s, -s, s, s, -1, 0, 0);
    addTriangle(-s, -s, -s, -s, s, s, -s, s, -s, -1, 0, 0);
    // Top face (Y+)
    addTriangle(-s, s, s, s, s, s, s, s, -s, 0, 1, 0);
    addTriangle(-s, s, s, s, s, -s, -s, s, -s, 0, 1, 0);
    // Bottom face (Y-)
    addTriangle(-s, -s, -s, s, -s, -s, s, -s, s, 0, -1, 0);
    addTriangle(-s, -s, -s, s, -s, s, -s, -s, s, 0, -1, 0);
    
    return vertices;
}

static std::vector<float> generateCubeWireframe(float size) {
    float s = size * 0.5f;
    return {
        // Bottom face
        -s, -s, -s,  s, -s, -s,
        s, -s, -s,  s, -s, s,
        s, -s, s,  -s, -s, s,
        -s, -s, s,  -s, -s, -s,
        // Top face
        -s, s, -s,  s, s, -s,
        s, s, -s,  s, s, s,
        s, s, s,  -s, s, s,
        -s, s, s,  -s, s, -s,
        // Vertical edges
        -s, -s, -s,  -s, s, -s,
        s, -s, -s,  s, s, -s,
        s, -s, s,  s, s, s,
        -s, -s, s,  -s, s, s
    };
}

static std::vector<float> generateSphereSolid(float radius, int segments) {
    std::vector<float> vertices;
    
    for (int i = 0; i < segments; i++) {
        float theta1 = 3.14159f * i / segments;
        float theta2 = 3.14159f * (i + 1) / segments;
        
        for (int j = 0; j < segments; j++) {
            float phi1 = 2.0f * 3.14159f * j / segments;
            float phi2 = 2.0f * 3.14159f * (j + 1) / segments;
            
            // Generate 4 vertices for quad
            auto getVertex = [&](float t, float p) -> std::tuple<float, float, float, float, float, float> {
                float x = radius * std::sin(t) * std::cos(p);
                float y = radius * std::cos(t);
                float z = radius * std::sin(t) * std::sin(p);
                float nx = x / radius;
                float ny = y / radius;
                float nz = z / radius;
                return {x, y, z, nx, ny, nz};
            };
            
            auto [x1, y1, z1, nx1, ny1, nz1] = getVertex(theta1, phi1);
            auto [x2, y2, z2, nx2, ny2, nz2] = getVertex(theta1, phi2);
            auto [x3, y3, z3, nx3, ny3, nz3] = getVertex(theta2, phi1);
            auto [x4, y4, z4, nx4, ny4, nz4] = getVertex(theta2, phi2);
            
            // Two triangles per quad
            vertices.insert(vertices.end(), {x1, y1, z1, nx1, ny1, nz1});
            vertices.insert(vertices.end(), {x2, y2, z2, nx2, ny2, nz2});
            vertices.insert(vertices.end(), {x3, y3, z3, nx3, ny3, nz3});
            
            vertices.insert(vertices.end(), {x2, y2, z2, nx2, ny2, nz2});
            vertices.insert(vertices.end(), {x4, y4, z4, nx4, ny4, nz4});
            vertices.insert(vertices.end(), {x3, y3, z3, nx3, ny3, nz3});
        }
    }
    
    return vertices;
}

static std::vector<float> generateSphereWireframe(float radius, int segments) {
    std::vector<float> vertices;
    
    // Latitude circles
    for (int i = 0; i <= segments; i++) {
        float theta = 3.14159f * i / segments;
        float y = radius * std::cos(theta);
        float r = radius * std::sin(theta);
        
        for (int j = 0; j < segments; j++) {
            float phi1 = 2.0f * 3.14159f * j / segments;
            float phi2 = 2.0f * 3.14159f * (j + 1) / segments;
            
            vertices.push_back(r * std::cos(phi1));
            vertices.push_back(y);
            vertices.push_back(r * std::sin(phi1));
            
            vertices.push_back(r * std::cos(phi2));
            vertices.push_back(y);
            vertices.push_back(r * std::sin(phi2));
        }
    }
    
    // Longitude circles
    for (int i = 0; i < segments; i++) {
        float phi = 2.0f * 3.14159f * i / segments;
        float cosPhi = std::cos(phi);
        float sinPhi = std::sin(phi);
        
        for (int j = 0; j < segments; j++) {
            float theta1 = 3.14159f * j / segments;
            float theta2 = 3.14159f * (j + 1) / segments;
            
            vertices.push_back(radius * std::sin(theta1) * cosPhi);
            vertices.push_back(radius * std::cos(theta1));
            vertices.push_back(radius * std::sin(theta1) * sinPhi);
            
            vertices.push_back(radius * std::sin(theta2) * cosPhi);
            vertices.push_back(radius * std::cos(theta2));
            vertices.push_back(radius * std::sin(theta2) * sinPhi);
        }
    }
    
    return vertices;
}

static std::vector<float> generateTorusSolid(float majorRadius, float minorRadius, int majorSegments, int minorSegments) {
    std::vector<float> vertices;
    
    for (int i = 0; i < majorSegments; i++) {
        float u1 = 2.0f * 3.14159f * i / majorSegments;
        float u2 = 2.0f * 3.14159f * (i + 1) / majorSegments;
        
        float cosU1 = std::cos(u1);
        float sinU1 = std::sin(u1);
        float cosU2 = std::cos(u2);
        float sinU2 = std::sin(u2);
        
        for (int j = 0; j < minorSegments; j++) {
            float v1 = 2.0f * 3.14159f * j / minorSegments;
            float v2 = 2.0f * 3.14159f * (j + 1) / minorSegments;
            
            auto getVertex = [&](float u, float v) -> std::tuple<float, float, float, float, float, float> {
                float cosV = std::cos(v);
                float sinV = std::sin(v);
                float cosU = std::cos(u);
                float sinU = std::sin(u);
                
                float x = (majorRadius + minorRadius * cosV) * cosU;
                float y = minorRadius * sinV;
                float z = (majorRadius + minorRadius * cosV) * sinU;
                
                // Normal
                float nx = cosV * cosU;
                float ny = sinV;
                float nz = cosV * sinU;
                
                return {x, y, z, nx, ny, nz};
            };
            
            auto [x1, y1, z1, nx1, ny1, nz1] = getVertex(u1, v1);
            auto [x2, y2, z2, nx2, ny2, nz2] = getVertex(u1, v2);
            auto [x3, y3, z3, nx3, ny3, nz3] = getVertex(u2, v1);
            auto [x4, y4, z4, nx4, ny4, nz4] = getVertex(u2, v2);
            
            // Two triangles per quad
            vertices.insert(vertices.end(), {x1, y1, z1, nx1, ny1, nz1});
            vertices.insert(vertices.end(), {x2, y2, z2, nx2, ny2, nz2});
            vertices.insert(vertices.end(), {x3, y3, z3, nx3, ny3, nz3});
            
            vertices.insert(vertices.end(), {x2, y2, z2, nx2, ny2, nz2});
            vertices.insert(vertices.end(), {x4, y4, z4, nx4, ny4, nz4});
            vertices.insert(vertices.end(), {x3, y3, z3, nx3, ny3, nz3});
        }
    }
    
    return vertices;
}

static std::vector<float> generateTorusWireframe(float majorRadius, float minorRadius, int majorSegments, int minorSegments) {
    std::vector<float> vertices;
    
    for (int i = 0; i < majorSegments; i++) {
        float u1 = 2.0f * 3.14159f * i / majorSegments;
        float u2 = 2.0f * 3.14159f * (i + 1) / majorSegments;
        
        float cosU1 = std::cos(u1);
        float sinU1 = std::sin(u1);
        float cosU2 = std::cos(u2);
        float sinU2 = std::sin(u2);
        
        for (int j = 0; j < minorSegments; j++) {
            float v1 = 2.0f * 3.14159f * j / minorSegments;
            float v2 = 2.0f * 3.14159f * (j + 1) / minorSegments;
            
            float cosV1 = std::cos(v1);
            float sinV1 = std::sin(v1);
            float cosV2 = std::cos(v2);
            float sinV2 = std::sin(v2);
            
            float x1 = (majorRadius + minorRadius * cosV1) * cosU1;
            float y1 = minorRadius * sinV1;
            float z1 = (majorRadius + minorRadius * cosV1) * sinU1;
            
            float x2 = (majorRadius + minorRadius * cosV2) * cosU1;
            float y2 = minorRadius * sinV2;
            float z2 = (majorRadius + minorRadius * cosV2) * sinU1;
            
            vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(z1);
            vertices.push_back(x2); vertices.push_back(y2); vertices.push_back(z2);
        }
    }
    
    // Torus circles
    for (int i = 0; i < majorSegments; i++) {
        float u = 2.0f * 3.14159f * i / majorSegments;
        float cosU = std::cos(u);
        float sinU = std::sin(u);
        
        for (int j = 0; j < minorSegments; j++) {
            float v1 = 2.0f * 3.14159f * j / minorSegments;
            float v2 = 2.0f * 3.14159f * (j + 1) / minorSegments;
            
            float x1 = (majorRadius + minorRadius * std::cos(v1)) * cosU;
            float y1 = minorRadius * std::sin(v1);
            float z1 = (majorRadius + minorRadius * std::cos(v1)) * sinU;
            
            float x2 = (majorRadius + minorRadius * std::cos(v2)) * cosU;
            float y2 = minorRadius * std::sin(v2);
            float z2 = (majorRadius + minorRadius * std::cos(v2)) * sinU;
            
            vertices.push_back(x1); vertices.push_back(y1); vertices.push_back(z1);
            vertices.push_back(x2); vertices.push_back(y2); vertices.push_back(z2);
        }
    }
    
    return vertices;
}

static std::vector<float> generateCylinderSolid(float radius, float height, int segments) {
    std::vector<float> vertices;
    float h = height * 0.5f;
    
    // Side faces
    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * 3.14159f * i / segments;
        float a2 = 2.0f * 3.14159f * (i + 1) / segments;
        
        float cos1 = std::cos(a1);
        float sin1 = std::sin(a1);
        float cos2 = std::cos(a2);
        float sin2 = std::sin(a2);
        
        float x1 = radius * cos1;
        float z1 = radius * sin1;
        float x2 = radius * cos2;
        float z2 = radius * sin2;
        
        // Normal for side face
        float nx1 = cos1;
        float nz1 = sin1;
        float nx2 = cos2;
        float nz2 = sin2;
        
        // Two triangles per segment
        vertices.insert(vertices.end(), {x1, -h, z1, nx1, 0, nz1});
        vertices.insert(vertices.end(), {x2, -h, z2, nx2, 0, nz2});
        vertices.insert(vertices.end(), {x1, h, z1, nx1, 0, nz1});
        
        vertices.insert(vertices.end(), {x2, -h, z2, nx2, 0, nz2});
        vertices.insert(vertices.end(), {x2, h, z2, nx2, 0, nz2});
        vertices.insert(vertices.end(), {x1, h, z1, nx1, 0, nz1});
    }
    
    // Top and bottom caps
    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * 3.14159f * i / segments;
        float a2 = 2.0f * 3.14159f * (i + 1) / segments;
        
        float cos1 = std::cos(a1);
        float sin1 = std::sin(a1);
        float cos2 = std::cos(a2);
        float sin2 = std::sin(a2);
        
        // Top cap
        vertices.insert(vertices.end(), {0, h, 0, 0, 1, 0});
        vertices.insert(vertices.end(), {radius * cos1, h, radius * sin1, 0, 1, 0});
        vertices.insert(vertices.end(), {radius * cos2, h, radius * sin2, 0, 1, 0});
        
        // Bottom cap
        vertices.insert(vertices.end(), {0, -h, 0, 0, -1, 0});
        vertices.insert(vertices.end(), {radius * cos2, -h, radius * sin2, 0, -1, 0});
        vertices.insert(vertices.end(), {radius * cos1, -h, radius * sin1, 0, -1, 0});
    }
    
    return vertices;
}

static std::vector<float> generateCylinderWireframe(float radius, float height, int segments) {
    std::vector<float> vertices;
    float h = height * 0.5f;
    
    // Bottom circle
    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * 3.14159f * i / segments;
        float a2 = 2.0f * 3.14159f * (i + 1) / segments;
        
        vertices.push_back(radius * std::cos(a1));
        vertices.push_back(-h);
        vertices.push_back(radius * std::sin(a1));
        
        vertices.push_back(radius * std::cos(a2));
        vertices.push_back(-h);
        vertices.push_back(radius * std::sin(a2));
    }
    
    // Top circle
    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * 3.14159f * i / segments;
        float a2 = 2.0f * 3.14159f * (i + 1) / segments;
        
        vertices.push_back(radius * std::cos(a1));
        vertices.push_back(h);
        vertices.push_back(radius * std::sin(a1));
        
        vertices.push_back(radius * std::cos(a2));
        vertices.push_back(h);
        vertices.push_back(radius * std::sin(a2));
    }
    
    // Vertical lines
    for (int i = 0; i < segments; i++) {
        float a = 2.0f * 3.14159f * i / segments;
        float x = radius * std::cos(a);
        float z = radius * std::sin(a);
        
        vertices.push_back(x); vertices.push_back(-h); vertices.push_back(z);
        vertices.push_back(x); vertices.push_back(h); vertices.push_back(z);
    }
    
    return vertices;
}

static std::vector<float> generatePlaneSolid(float size) {
    float s = size * 0.5f;
    // Generate plane with two triangles
    return {
        // Triangle 1
        -s, 0, -s,  0, 1, 0,
        s, 0, -s,  0, 1, 0,
        s, 0, s,  0, 1, 0,
        // Triangle 2
        -s, 0, -s,  0, 1, 0,
        s, 0, s,  0, 1, 0,
        -s, 0, s,  0, 1, 0
    };
}

static std::vector<float> generatePlaneWireframe(float size) {
    float s = size * 0.5f;
    return {
        // Square outline
        -s, 0, -s,  s, 0, -s,
        s, 0, -s,  s, 0, s,
        s, 0, s,  -s, 0, s,
        -s, 0, s,  -s, 0, -s,
        // Diagonal lines
        -s, 0, -s,  s, 0, s,
        s, 0, -s,  -s, 0, s
    };
}

// Wireframe line list is 6 floats per segment (x1,y1,z1, x2,y2,z2). Expand to 6 floats per vertex (pos+normal) for grid shader.
static std::vector<float> wireframeLinesToVertices(const std::vector<float>& lines, float nx = 0.0f, float ny = 1.0f, float nz = 0.0f) {
    std::vector<float> out;
    out.reserve(lines.size() * 2);
    for (size_t i = 0; i + 5 < lines.size(); i += 6) {
        out.insert(out.end(), {lines[i], lines[i+1], lines[i+2], nx, ny, nz});
        out.insert(out.end(), {lines[i+3], lines[i+4], lines[i+5], nx, ny, nz});
    }
    return out;
}

// Plane wireframe: quad outline only (no diagonals).
static std::vector<float> generatePlaneWireframeQuads(float size) {
    float s = size * 0.5f;
    return {
        -s, 0, -s,  s, 0, -s,
        s, 0, -s,  s, 0, s,
        s, 0, s,  -s, 0, s,
        -s, 0, s,  -s, 0, -s
    };
}

// --- Parametric box (generalizes cube) ---
static std::vector<float> generateBoxSolid(float w, float h, float d) {
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    std::vector<float> v;
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
        // Two triangles: a-b-c, a-c-d
        for (auto& p : {a, b, c, a, c, d}) {
            v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
            v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
        }
    };
    // Front (+Z)
    quad({-hw,-hh,hd},{hw,-hh,hd},{hw,hh,hd},{-hw,hh,hd},{0,0,1});
    // Back (-Z)
    quad({hw,-hh,-hd},{-hw,-hh,-hd},{-hw,hh,-hd},{hw,hh,-hd},{0,0,-1});
    // Right (+X)
    quad({hw,-hh,hd},{hw,-hh,-hd},{hw,hh,-hd},{hw,hh,hd},{1,0,0});
    // Left (-X)
    quad({-hw,-hh,-hd},{-hw,-hh,hd},{-hw,hh,hd},{-hw,hh,-hd},{-1,0,0});
    // Top (+Y)
    quad({-hw,hh,hd},{hw,hh,hd},{hw,hh,-hd},{-hw,hh,-hd},{0,1,0});
    // Bottom (-Y)
    quad({-hw,-hh,-hd},{hw,-hh,-hd},{hw,-hh,hd},{-hw,-hh,hd},{0,-1,0});
    return v;
}

static std::vector<float> generateBoxWireframe(float w, float h, float d) {
    float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;
    return {
        -hw,-hh,-hd, hw,-hh,-hd,  hw,-hh,-hd, hw,hh,-hd,
        hw,hh,-hd, -hw,hh,-hd,  -hw,hh,-hd, -hw,-hh,-hd,
        -hw,-hh,hd, hw,-hh,hd,  hw,-hh,hd, hw,hh,hd,
        hw,hh,hd, -hw,hh,hd,  -hw,hh,hd, -hw,-hh,hd,
        -hw,-hh,-hd, -hw,-hh,hd,  hw,-hh,-hd, hw,-hh,hd,
        hw,hh,-hd, hw,hh,hd,  -hw,hh,-hd, -hw,hh,hd
    };
}

// --- Annular ring (for Aperture) ---
static std::vector<float> generateAnnularRingSolid(float outerR, float innerR, float height, int segments) {
    std::vector<float> v;
    float halfH = height * 0.5f;
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * 3.14159265f * i / segments;
        float a1 = 2.0f * 3.14159265f * (i + 1) / segments;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);

        // Outer wall (normals pointing out)
        glm::vec3 n0(c0, 0, s0), n1(c1, 0, s1);
        glm::vec3 ot0(outerR*c0, -halfH, outerR*s0), ot1(outerR*c1, -halfH, outerR*s1);
        glm::vec3 ob0(outerR*c0, halfH, outerR*s0), ob1(outerR*c1, halfH, outerR*s1);
        // tri 1
        for (auto& p : {ot0, ot1, ob1}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); glm::vec3 n = (p==ot0||p==ob0) ? n0 : n1; v.push_back(n.x); v.push_back(n.y); v.push_back(n.z); }
        for (auto& p : {ot0, ob1, ob0}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); glm::vec3 n = (p==ot0||p==ob0) ? n0 : n1; v.push_back(n.x); v.push_back(n.y); v.push_back(n.z); }

        // Inner wall (normals pointing in)
        glm::vec3 ni0(-c0, 0, -s0), ni1(-c1, 0, -s1);
        glm::vec3 it0(innerR*c0, -halfH, innerR*s0), it1(innerR*c1, -halfH, innerR*s1);
        glm::vec3 ib0(innerR*c0, halfH, innerR*s0), ib1(innerR*c1, halfH, innerR*s1);
        for (auto& p : {it1, it0, ib0}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); glm::vec3 n = (p==it0||p==ib0) ? ni0 : ni1; v.push_back(n.x); v.push_back(n.y); v.push_back(n.z); }
        for (auto& p : {it1, ib0, ib1}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); glm::vec3 n = (p==it0||p==ib0) ? ni0 : ni1; v.push_back(n.x); v.push_back(n.y); v.push_back(n.z); }

        // Top cap (Y = +halfH, normal up)
        glm::vec3 nu(0, 1, 0);
        for (auto& p : {ob0, ob1, ib1}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); v.push_back(nu.x); v.push_back(nu.y); v.push_back(nu.z); }
        for (auto& p : {ob0, ib1, ib0}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); v.push_back(nu.x); v.push_back(nu.y); v.push_back(nu.z); }

        // Bottom cap (Y = -halfH, normal down)
        glm::vec3 nd(0, -1, 0);
        for (auto& p : {ot1, ot0, it0}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); v.push_back(nd.x); v.push_back(nd.y); v.push_back(nd.z); }
        for (auto& p : {ot1, it0, it1}) { v.push_back(p.x); v.push_back(p.y); v.push_back(p.z); v.push_back(nd.x); v.push_back(nd.y); v.push_back(nd.z); }
    }
    return v;
}

static std::vector<float> generateAnnularRingWireframe(float outerR, float innerR, float height, int segments) {
    std::vector<float> v;
    float halfH = height * 0.5f;
    for (int i = 0; i < segments; i++) {
        float a0 = 2.0f * 3.14159265f * i / segments;
        float a1 = 2.0f * 3.14159265f * (i + 1) / segments;
        float c0 = cosf(a0), s0 = sinf(a0), c1 = cosf(a1), s1 = sinf(a1);
        // Outer top circle
        v.push_back(outerR*c0); v.push_back(halfH); v.push_back(outerR*s0);
        v.push_back(outerR*c1); v.push_back(halfH); v.push_back(outerR*s1);
        // Outer bottom circle
        v.push_back(outerR*c0); v.push_back(-halfH); v.push_back(outerR*s0);
        v.push_back(outerR*c1); v.push_back(-halfH); v.push_back(outerR*s1);
        // Inner top circle
        v.push_back(innerR*c0); v.push_back(halfH); v.push_back(innerR*s0);
        v.push_back(innerR*c1); v.push_back(halfH); v.push_back(innerR*s1);
        // Inner bottom circle
        v.push_back(innerR*c0); v.push_back(-halfH); v.push_back(innerR*s0);
        v.push_back(innerR*c1); v.push_back(-halfH); v.push_back(innerR*s1);
    }
    // Vertical lines connecting outer to inner at a few positions
    for (int i = 0; i < segments; i += segments / 4) {
        float a = 2.0f * 3.14159265f * i / segments;
        float c = cosf(a), s = sinf(a);
        v.push_back(outerR*c); v.push_back(halfH); v.push_back(outerR*s);
        v.push_back(outerR*c); v.push_back(-halfH); v.push_back(outerR*s);
        v.push_back(innerR*c); v.push_back(halfH); v.push_back(innerR*s);
        v.push_back(innerR*c); v.push_back(-halfH); v.push_back(innerR*s);
    }
    return v;
}

// --- Triangular prism (for Prism and PrismRA) ---
static std::vector<float> generateTriangularPrismSolid(float sideLen, float depth, bool rightAngle) {
    std::vector<float> v;
    float hd = depth * 0.5f;

    // Triangle vertices in XY plane
    glm::vec3 p0, p1, p2;
    if (rightAngle) {
        // Right-angle: 45-45-90, right angle at origin
        float leg = sideLen;
        p0 = {-leg*0.5f, -leg*0.5f, 0};
        p1 = { leg*0.5f, -leg*0.5f, 0};
        p2 = {-leg*0.5f,  leg*0.5f, 0};
    } else {
        // Equilateral triangle centered
        float h = sideLen * 0.866f; // sqrt(3)/2
        float cy = h / 3.0f; // centroid y offset from base
        p0 = {0, h - cy, 0};
        p1 = {-sideLen*0.5f, -cy, 0};
        p2 = { sideLen*0.5f, -cy, 0};
    }

    auto addTri = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 n) {
        for (auto& p : {a, b, c}) {
            v.push_back(p.x); v.push_back(p.y); v.push_back(p.z);
            v.push_back(n.x); v.push_back(n.y); v.push_back(n.z);
        }
    };
    auto addQuad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d, glm::vec3 n) {
        addTri(a, b, c, n);
        addTri(a, c, d, n);
    };

    // Front cap (+Z)
    glm::vec3 f0 = p0 + glm::vec3(0,0,hd), f1 = p1 + glm::vec3(0,0,hd), f2 = p2 + glm::vec3(0,0,hd);
    addTri(f0, f1, f2, {0, 0, 1});

    // Back cap (-Z)
    glm::vec3 b0 = p0 + glm::vec3(0,0,-hd), b1 = p1 + glm::vec3(0,0,-hd), b2 = p2 + glm::vec3(0,0,-hd);
    addTri(b0, b2, b1, {0, 0, -1});

    // Side faces (3 edges of the triangle, each extruded)
    glm::vec3 triPts[3] = {p0, p1, p2};
    for (int i = 0; i < 3; i++) {
        glm::vec3 a = triPts[i], b = triPts[(i+1)%3];
        glm::vec3 af = a + glm::vec3(0,0,hd), bf = b + glm::vec3(0,0,hd);
        glm::vec3 ab = a + glm::vec3(0,0,-hd), bb = b + glm::vec3(0,0,-hd);
        // Normal = cross(edge along triangle, Z axis)
        glm::vec3 edge = b - a;
        glm::vec3 n = glm::normalize(glm::cross(edge, glm::vec3(0,0,1)));
        addQuad(af, bf, bb, ab, n);
    }
    return v;
}

static std::vector<float> generateTriangularPrismWireframe(float sideLen, float depth, bool rightAngle) {
    std::vector<float> v;
    float hd = depth * 0.5f;

    glm::vec3 p0, p1, p2;
    if (rightAngle) {
        float leg = sideLen;
        p0 = {-leg*0.5f, -leg*0.5f, 0};
        p1 = { leg*0.5f, -leg*0.5f, 0};
        p2 = {-leg*0.5f,  leg*0.5f, 0};
    } else {
        float h = sideLen * 0.866f;
        float cy = h / 3.0f;
        p0 = {0, h - cy, 0};
        p1 = {-sideLen*0.5f, -cy, 0};
        p2 = { sideLen*0.5f, -cy, 0};
    }

    glm::vec3 pts[3] = {p0, p1, p2};
    // Front face edges
    for (int i = 0; i < 3; i++) {
        glm::vec3 a = pts[i] + glm::vec3(0,0,hd);
        glm::vec3 b = pts[(i+1)%3] + glm::vec3(0,0,hd);
        v.push_back(a.x); v.push_back(a.y); v.push_back(a.z);
        v.push_back(b.x); v.push_back(b.y); v.push_back(b.z);
    }
    // Back face edges
    for (int i = 0; i < 3; i++) {
        glm::vec3 a = pts[i] + glm::vec3(0,0,-hd);
        glm::vec3 b = pts[(i+1)%3] + glm::vec3(0,0,-hd);
        v.push_back(a.x); v.push_back(a.y); v.push_back(a.z);
        v.push_back(b.x); v.push_back(b.y); v.push_back(b.z);
    }
    // Connecting edges
    for (int i = 0; i < 3; i++) {
        glm::vec3 a = pts[i] + glm::vec3(0,0,hd);
        glm::vec3 b = pts[i] + glm::vec3(0,0,-hd);
        v.push_back(a.x); v.push_back(a.y); v.push_back(a.z);
        v.push_back(b.x); v.push_back(b.y); v.push_back(b.z);
    }
    return v;
}

// --- Composite shape utilities ---
static void offsetVertices(std::vector<float>& v, float dx, float dy, float dz) {
    for (size_t i = 0; i < v.size(); i += 6) {
        v[i] += dx; v[i + 1] += dy; v[i + 2] += dz;
    }
}

static void appendVertices(std::vector<float>& dst, const std::vector<float>& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

static void offsetWireframeLines(std::vector<float>& v, float dx, float dy, float dz) {
    for (size_t i = 0; i + 5 < v.size(); i += 6) {
        v[i] += dx; v[i + 1] += dy; v[i + 2] += dz;
        v[i + 3] += dx; v[i + 4] += dy; v[i + 5] += dz;
    }
}

// --- Laser: elongated cylinder body + smaller nose cylinder ---
static std::vector<float> generateLaserSolid() {
    auto body = generateCylinderSolid(0.25f, 1.4f, 16);
    auto nose = generateCylinderSolid(0.12f, 0.3f, 12);
    offsetVertices(nose, 0.0f, 0.85f, 0.0f);
    appendVertices(body, nose);
    offsetVertices(body, 0.0f, -0.15f, 0.0f);
    return body;
}

static std::vector<float> generateLaserWireframe() {
    auto body = generateCylinderWireframe(0.25f, 1.4f, 16);
    auto nose = generateCylinderWireframe(0.12f, 0.3f, 12);
    offsetWireframeLines(nose, 0.0f, 0.85f, 0.0f);
    body.insert(body.end(), nose.begin(), nose.end());
    offsetWireframeLines(body, 0.0f, -0.15f, 0.0f);
    return body;
}

// --- Mirror: thin disc with concave front face ---
static std::vector<float> generateMirrorDiscSolid(float radius, float capDepth, float thickness, int segments) {
    std::vector<float> v;
    float halfT = thickness * 0.5f;
    const float PI = 3.14159265f;

    auto pushVN = [&](const glm::vec3& pos, const glm::vec3& norm) {
        v.push_back(pos.x); v.push_back(pos.y); v.push_back(pos.z);
        v.push_back(norm.x); v.push_back(norm.y); v.push_back(norm.z);
    };

    float R = (radius * radius + capDepth * capDepth) / (2.0f * capDepth);
    float zc = halfT - capDepth + R;
    float thetaMax = std::asin(std::min(radius / R, 1.0f));
    int rings = 8;

    // Front face: concave spherical cap (normals point into concavity, toward +Z)
    for (int i = 0; i < rings; i++) {
        float t0 = thetaMax * i / rings;
        float t1 = thetaMax * (i + 1) / rings;
        for (int j = 0; j < segments; j++) {
            float p0 = 2.0f * PI * j / segments;
            float p1 = 2.0f * PI * (j + 1) / segments;

            auto pos = [&](float t, float p) {
                return glm::vec3(R * sinf(t) * cosf(p), R * sinf(t) * sinf(p), zc - R * cosf(t));
            };
            auto norm = [&](float t, float p) {
                return glm::vec3(-sinf(t) * cosf(p), -sinf(t) * sinf(p), cosf(t));
            };

            pushVN(pos(t0,p0), norm(t0,p0));
            pushVN(pos(t1,p0), norm(t1,p0));
            pushVN(pos(t1,p1), norm(t1,p1));

            pushVN(pos(t0,p0), norm(t0,p0));
            pushVN(pos(t1,p1), norm(t1,p1));
            pushVN(pos(t0,p1), norm(t0,p1));
        }
    }

    // Back face: flat disc at Z = -halfT
    glm::vec3 backN(0, 0, -1);
    for (int j = 0; j < segments; j++) {
        float p0 = 2.0f * PI * j / segments;
        float p1 = 2.0f * PI * (j + 1) / segments;
        pushVN(glm::vec3(0, 0, -halfT), backN);
        pushVN(glm::vec3(radius * cosf(p1), radius * sinf(p1), -halfT), backN);
        pushVN(glm::vec3(radius * cosf(p0), radius * sinf(p0), -halfT), backN);
    }

    // Rim: connecting front rim to back edge
    float frontRimZ = zc - R * cosf(thetaMax);
    for (int j = 0; j < segments; j++) {
        float p0 = 2.0f * PI * j / segments;
        float p1 = 2.0f * PI * (j + 1) / segments;
        float c0 = cosf(p0), s0 = sinf(p0), c1 = cosf(p1), s1 = sinf(p1);
        glm::vec3 n0(c0, s0, 0), n1(c1, s1, 0);

        pushVN(glm::vec3(radius*c0, radius*s0, frontRimZ), n0);
        pushVN(glm::vec3(radius*c1, radius*s1, frontRimZ), n1);
        pushVN(glm::vec3(radius*c1, radius*s1, -halfT), n1);

        pushVN(glm::vec3(radius*c0, radius*s0, frontRimZ), n0);
        pushVN(glm::vec3(radius*c1, radius*s1, -halfT), n1);
        pushVN(glm::vec3(radius*c0, radius*s0, -halfT), n0);
    }

    return v;
}

// --- Biconvex lens: two spherical caps meeting at rim ---
static std::vector<float> generateBiconvexLensSolid(float radius, float centerThick, float edgeThick, int segments, int rings) {
    std::vector<float> v;
    const float PI = 3.14159265f;
    float sag = (centerThick - edgeThick) * 0.5f;
    float R = (radius * radius + sag * sag) / (2.0f * sag);
    float thetaMax = std::asin(std::min(radius / R, 1.0f));
    float halfEdge = edgeThick * 0.5f;
    float zcFront = -R + centerThick * 0.5f;
    float zcBack = R - centerThick * 0.5f;

    auto pushVN = [&](const glm::vec3& pos, const glm::vec3& norm) {
        v.push_back(pos.x); v.push_back(pos.y); v.push_back(pos.z);
        v.push_back(norm.x); v.push_back(norm.y); v.push_back(norm.z);
    };

    // Front cap (+Z side)
    for (int i = 0; i < rings; i++) {
        float t0 = thetaMax * i / rings;
        float t1 = thetaMax * (i + 1) / rings;
        for (int j = 0; j < segments; j++) {
            float p0 = 2.0f * PI * j / segments;
            float p1 = 2.0f * PI * (j + 1) / segments;

            auto fPos = [&](float t, float p) {
                return glm::vec3(R*sinf(t)*cosf(p), R*sinf(t)*sinf(p), zcFront + R*cosf(t));
            };
            auto fNorm = [&](float t, float p) {
                return glm::vec3(sinf(t)*cosf(p), sinf(t)*sinf(p), cosf(t));
            };

            pushVN(fPos(t0,p0), fNorm(t0,p0));
            pushVN(fPos(t1,p0), fNorm(t1,p0));
            pushVN(fPos(t1,p1), fNorm(t1,p1));

            pushVN(fPos(t0,p0), fNorm(t0,p0));
            pushVN(fPos(t1,p1), fNorm(t1,p1));
            pushVN(fPos(t0,p1), fNorm(t0,p1));
        }
    }

    // Back cap (-Z side)
    for (int i = 0; i < rings; i++) {
        float t0 = thetaMax * i / rings;
        float t1 = thetaMax * (i + 1) / rings;
        for (int j = 0; j < segments; j++) {
            float p0 = 2.0f * PI * j / segments;
            float p1 = 2.0f * PI * (j + 1) / segments;

            auto bPos = [&](float t, float p) {
                return glm::vec3(R*sinf(t)*cosf(p), R*sinf(t)*sinf(p), zcBack - R*cosf(t));
            };
            auto bNorm = [&](float t, float p) {
                return glm::vec3(sinf(t)*cosf(p), sinf(t)*sinf(p), -cosf(t));
            };

            pushVN(bPos(t0,p0), bNorm(t0,p0));
            pushVN(bPos(t1,p1), bNorm(t1,p1));
            pushVN(bPos(t1,p0), bNorm(t1,p0));

            pushVN(bPos(t0,p0), bNorm(t0,p0));
            pushVN(bPos(t0,p1), bNorm(t0,p1));
            pushVN(bPos(t1,p1), bNorm(t1,p1));
        }
    }

    // Rim strip
    for (int j = 0; j < segments; j++) {
        float p0 = 2.0f * PI * j / segments;
        float p1 = 2.0f * PI * (j + 1) / segments;
        float c0 = cosf(p0), s0 = sinf(p0), c1 = cosf(p1), s1 = sinf(p1);
        glm::vec3 n0(c0, s0, 0), n1(c1, s1, 0);
        glm::vec3 ft0(radius*c0, radius*s0, halfEdge);
        glm::vec3 ft1(radius*c1, radius*s1, halfEdge);
        glm::vec3 bt0(radius*c0, radius*s0, -halfEdge);
        glm::vec3 bt1(radius*c1, radius*s1, -halfEdge);

        pushVN(ft0, n0); pushVN(ft1, n1); pushVN(bt1, n1);
        pushVN(ft0, n0); pushVN(bt1, n1); pushVN(bt0, n0);
    }

    return v;
}

static std::vector<float> generateBiconvexLensWireframe(float radius, float centerThick, float edgeThick, int segments) {
    std::vector<float> v;
    const float PI = 3.14159265f;
    float sag = (centerThick - edgeThick) * 0.5f;
    float R = (radius * radius + sag * sag) / (2.0f * sag);
    float thetaMax = std::asin(std::min(radius / R, 1.0f));
    float zcFront = -R + centerThick * 0.5f;
    float zcBack = R - centerThick * 0.5f;

    // Equator circle at rim
    for (int j = 0; j < segments; j++) {
        float p0 = 2.0f * PI * j / segments;
        float p1 = 2.0f * PI * (j + 1) / segments;
        v.insert(v.end(), {radius*cosf(p0), radius*sinf(p0), 0.0f,
                           radius*cosf(p1), radius*sinf(p1), 0.0f});
    }

    // Profile arcs at 4 meridians
    int arcSegs = 16;
    for (int m = 0; m < 4; m++) {
        float phi = PI * 0.5f * m;
        float cp = cosf(phi), sp = sinf(phi);

        for (int i = 0; i < arcSegs; i++) {
            float t0 = thetaMax * i / arcSegs;
            float t1 = thetaMax * (i + 1) / arcSegs;
            // Front arc
            v.insert(v.end(), {R*sinf(t0)*cp, R*sinf(t0)*sp, zcFront + R*cosf(t0),
                               R*sinf(t1)*cp, R*sinf(t1)*sp, zcFront + R*cosf(t1)});
            // Back arc
            v.insert(v.end(), {R*sinf(t0)*cp, R*sinf(t0)*sp, zcBack - R*cosf(t0),
                               R*sinf(t1)*cp, R*sinf(t1)*sp, zcBack - R*cosf(t1)});
        }
    }

    return v;
}

// --- Detector: box body (replaces flat plane) ---
static std::vector<float> generateDetectorSolid() {
    return generateBoxSolid(0.7f, 0.7f, 0.25f);
}

static std::vector<float> generateDetectorWireframe() {
    return generateBoxWireframe(0.7f, 0.7f, 0.25f);
}

// --- Mount: post cylinder on base plate ---
static std::vector<float> generateMountSolid() {
    auto post = generateCylinderSolid(0.06f, 1.1f, 12);
    auto base = generateCylinderSolid(0.28f, 0.07f, 16);
    offsetVertices(base, 0.0f, -0.585f, 0.0f);
    appendVertices(post, base);
    return post;
}

static std::vector<float> generateMountWireframe() {
    auto post = generateCylinderWireframe(0.06f, 1.1f, 12);
    auto base = generateCylinderWireframe(0.28f, 0.07f, 16);
    offsetWireframeLines(base, 0.0f, -0.585f, 0.0f);
    post.insert(post.end(), base.begin(), base.end());
    return post;
}

// --- Fiber coupler: body cylinder + fiber stub ---
static std::vector<float> generateFiberCouplerSolid() {
    auto body = generateCylinderSolid(0.18f, 0.45f, 16);
    auto stub = generateCylinderSolid(0.05f, 0.35f, 8);
    offsetVertices(stub, 0.0f, 0.4f, 0.0f);
    appendVertices(body, stub);
    offsetVertices(body, 0.0f, -0.175f, 0.0f);
    return body;
}

static std::vector<float> generateFiberCouplerWireframe() {
    auto body = generateCylinderWireframe(0.18f, 0.45f, 16);
    auto stub = generateCylinderWireframe(0.05f, 0.35f, 8);
    offsetWireframeLines(stub, 0.0f, 0.4f, 0.0f);
    body.insert(body.end(), stub.begin(), stub.end());
    offsetWireframeLines(body, 0.0f, -0.175f, 0.0f);
    return body;
}

static std::vector<float> generateMirrorDiscWireframe(float radius, float capDepth, float thickness, int segments) {
    std::vector<float> v;
    float halfT = thickness * 0.5f;
    const float PI = 3.14159265f;
    float R = (radius * radius + capDepth * capDepth) / (2.0f * capDepth);
    float zc = halfT - capDepth + R;
    float thetaMax = std::asin(std::min(radius / R, 1.0f));
    float frontZ = zc - R * cosf(thetaMax);

    // Front rim circle
    for (int j = 0; j < segments; j++) {
        float p0 = 2.0f * PI * j / segments;
        float p1 = 2.0f * PI * (j + 1) / segments;
        v.insert(v.end(), {radius*cosf(p0), radius*sinf(p0), frontZ,
                           radius*cosf(p1), radius*sinf(p1), frontZ});
    }
    // Back circle
    for (int j = 0; j < segments; j++) {
        float p0 = 2.0f * PI * j / segments;
        float p1 = 2.0f * PI * (j + 1) / segments;
        v.insert(v.end(), {radius*cosf(p0), radius*sinf(p0), -halfT,
                           radius*cosf(p1), radius*sinf(p1), -halfT});
    }
    // 4 connecting lines
    for (int j = 0; j < 4; j++) {
        float p = PI * 0.5f * j;
        float x = radius * cosf(p), y = radius * sinf(p);
        v.insert(v.end(), {x, y, frontZ, x, y, -halfT});
    }
    // Concave profile arcs along X and Y axes
    int arcSegs = 12;
    for (int axis = 0; axis < 2; axis++) {
        for (int i = 0; i < arcSegs; i++) {
            float t0 = thetaMax * i / arcSegs;
            float t1 = thetaMax * (i + 1) / arcSegs;
            float r0 = R * sinf(t0), r1 = R * sinf(t1);
            float z0 = zc - R * cosf(t0), z1 = zc - R * cosf(t1);
            if (axis == 0) {
                v.insert(v.end(), {r0, 0, z0, r1, 0, z1});
                v.insert(v.end(), {-r0, 0, z0, -r1, 0, z1});
            } else {
                v.insert(v.end(), {0, r0, z0, 0, r1, z1});
                v.insert(v.end(), {0, -r0, z0, 0, -r1, z1});
            }
        }
    }
    return v;
}

void Viewport::initPrototypeGeometry() {
    if (prototypesInitialized) return;

    // Solid geometry for each built-in ElementType
    prototypeGeometry[(int)ElementType::Laser]        = createCachedMesh(generateLaserSolid());
    prototypeGeometry[(int)ElementType::Mirror]       = createCachedMesh(generateMirrorDiscSolid(0.5f, 0.05f, 0.08f, 24));
    prototypeGeometry[(int)ElementType::Lens]         = createCachedMesh(generateBiconvexLensSolid(0.5f, 0.22f, 0.03f, 24, 8));
    prototypeGeometry[(int)ElementType::BeamSplitter] = createCachedMesh(generateBoxSolid(0.8f, 0.8f, 0.8f));
    prototypeGeometry[(int)ElementType::Detector]     = createCachedMesh(generateDetectorSolid());
    prototypeGeometry[(int)ElementType::Filter]       = createCachedMesh(generateCylinderSolid(0.5f, 0.05f, 24));
    prototypeGeometry[(int)ElementType::Aperture]     = createCachedMesh(generateAnnularRingSolid(0.5f, 0.15f, 0.06f, 24));
    prototypeGeometry[(int)ElementType::Prism]        = createCachedMesh(generateTriangularPrismSolid(1.0f, 1.0f, false));
    prototypeGeometry[(int)ElementType::PrismRA]      = createCachedMesh(generateTriangularPrismSolid(1.0f, 1.0f, true));
    prototypeGeometry[(int)ElementType::Grating]      = createCachedMesh(generateBoxSolid(1.0f, 1.0f, 0.04f));
    prototypeGeometry[(int)ElementType::FiberCoupler] = createCachedMesh(generateFiberCouplerSolid());
    prototypeGeometry[(int)ElementType::Screen]       = createCachedMesh(generateBoxSolid(1.5f, 2.0f, 0.04f));
    prototypeGeometry[(int)ElementType::Mount]        = createCachedMesh(generateMountSolid());

    // Wireframe geometry (needs pos+normal format for the shader)
    prototypeWireframe[(int)ElementType::Laser]        = createCachedMesh(wireframeLinesToVertices(generateLaserWireframe()));
    prototypeWireframe[(int)ElementType::Mirror]       = createCachedMesh(wireframeLinesToVertices(generateMirrorDiscWireframe(0.5f, 0.05f, 0.08f, 24)));
    prototypeWireframe[(int)ElementType::Lens]         = createCachedMesh(wireframeLinesToVertices(generateBiconvexLensWireframe(0.5f, 0.22f, 0.03f, 24)));
    prototypeWireframe[(int)ElementType::BeamSplitter] = createCachedMesh(wireframeLinesToVertices(generateBoxWireframe(0.8f, 0.8f, 0.8f)));
    prototypeWireframe[(int)ElementType::Detector]     = createCachedMesh(wireframeLinesToVertices(generateDetectorWireframe()));
    prototypeWireframe[(int)ElementType::Filter]       = createCachedMesh(wireframeLinesToVertices(generateCylinderWireframe(0.5f, 0.05f, 24)));
    prototypeWireframe[(int)ElementType::Aperture]     = createCachedMesh(wireframeLinesToVertices(generateAnnularRingWireframe(0.5f, 0.15f, 0.06f, 24)));
    prototypeWireframe[(int)ElementType::Prism]        = createCachedMesh(wireframeLinesToVertices(generateTriangularPrismWireframe(1.0f, 1.0f, false)));
    prototypeWireframe[(int)ElementType::PrismRA]      = createCachedMesh(wireframeLinesToVertices(generateTriangularPrismWireframe(1.0f, 1.0f, true)));
    prototypeWireframe[(int)ElementType::Grating]      = createCachedMesh(wireframeLinesToVertices(generateBoxWireframe(1.0f, 1.0f, 0.04f)));
    prototypeWireframe[(int)ElementType::FiberCoupler] = createCachedMesh(wireframeLinesToVertices(generateFiberCouplerWireframe()));
    prototypeWireframe[(int)ElementType::Screen]       = createCachedMesh(wireframeLinesToVertices(generateBoxWireframe(1.5f, 2.0f, 0.04f)));
    prototypeWireframe[(int)ElementType::Mount]        = createCachedMesh(wireframeLinesToVertices(generateMountWireframe()));

    prototypesInitialized = true;
}

void Viewport::renderScene(Scene* scene, bool forExport) {
    if (!scene) return;

    if (!prototypesInitialized) initPrototypeGeometry();

    // Clean up stale mesh cache entries (for deleted ImportedMesh elements)
    if (!meshCache.empty()) {
        const auto& elements = scene->getElements();
        auto it = meshCache.begin();
        while (it != meshCache.end()) {
            bool found = false;
            for (const auto& elem : elements) {
                if (elem->id == it->first) { found = true; break; }
            }
            if (!found) {
                deleteCachedMesh(it->second);
                it = meshCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Disable face culling for solid elements — generators have mixed winding
    // conventions; per-vertex normals handle lighting correctly, and depth
    // testing is sufficient for correct visual ordering.
    glDisable(GL_CULL_FACE);

    bool isSchematic = style && style->renderMode == RenderMode::Schematic;
    bool isPresentation = style && style->renderMode == RenderMode::Presentation;

    // Choose shader based on render mode
    Shader& activeShader = isPresentation ? materialShader : gridShader;

    activeShader.use();
    activeShader.setMat4("uView", camera.getViewMatrix());
    activeShader.setMat4("uProjection", camera.getProjectionMatrix());
    // Set lighting uniforms from style (Schematic: fully flat, no specular)
    if (isSchematic) {
        activeShader.setFloat("uAmbientStrength", 1.0f);
        activeShader.setFloat("uSpecularStrength", 0.0f);
        activeShader.setFloat("uShininess", 1.0f);
    } else {
        activeShader.setFloat("uAmbientStrength", style ? style->ambientStrength : 0.14f);
        activeShader.setFloat("uSpecularStrength", style ? style->specularStrength : 0.55f);
        activeShader.setFloat("uShininess", style ? style->specularShininess : 48.0f);
    }

    // HDRI environment map (Presentation mode only)
    if (isPresentation && style) {
        if (!style->hdriPath.empty()) {
            loadHdriTexture(style->hdriPath);
        } else if (hdriTexture != 0) {
            destroyHdriTexture();
        }
        if (hdriTexture != 0) {
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, hdriTexture);
            activeShader.setInt("uEnvMap", 1);
            activeShader.setBool("uHasEnvMap", true);
            activeShader.setFloat("uEnvIntensity", style->hdriIntensity);
            activeShader.setFloat("uEnvRotation", glm::radians(style->hdriRotation));
            glActiveTexture(GL_TEXTURE0);
        } else {
            activeShader.setBool("uHasEnvMap", false);
        }
    }

    // In Presentation mode, collect transparent elements for a second pass
    struct TransparentDraw {
        const Element* elem;
        CachedMesh* mesh;
        glm::vec3 color;
        bool isSelected;
    };
    std::vector<TransparentDraw> transparentElements;

    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;

        glm::mat4 model = elem->transform.getMatrix();
        activeShader.setMat4("uModel", model);

        // Determine color and which cached mesh to use
        glm::vec3 color;
        CachedMesh* solidMesh = nullptr;

        int typeIdx = static_cast<int>(elem->type);
        if (style && typeIdx >= 0 && typeIdx < kElementTypeCount) {
            color = style->elementColors[typeIdx];
        } else {
            switch (elem->type) {
                case ElementType::Laser:        color = glm::vec3(1.0f, 0.2f, 0.2f); break;
                case ElementType::Mirror:       color = glm::vec3(0.8f, 0.8f, 0.9f); break;
                case ElementType::Lens:         color = glm::vec3(0.7f, 0.9f, 1.0f); break;
                case ElementType::BeamSplitter: color = glm::vec3(0.9f, 0.9f, 0.7f); break;
                case ElementType::Detector:     color = glm::vec3(0.2f, 1.0f, 0.2f); break;
                case ElementType::Filter:       color = glm::vec3(0.6f, 0.4f, 0.8f); break;
                case ElementType::Aperture:     color = glm::vec3(0.8f, 0.6f, 0.3f); break;
                case ElementType::Prism:        color = glm::vec3(0.5f, 0.8f, 0.9f); break;
                case ElementType::PrismRA:      color = glm::vec3(0.5f, 0.8f, 0.9f); break;
                case ElementType::Grating:      color = glm::vec3(0.7f, 0.5f, 0.3f); break;
                case ElementType::FiberCoupler: color = glm::vec3(1.0f, 0.6f, 0.2f); break;
                case ElementType::Screen:       color = glm::vec3(0.3f, 0.8f, 0.3f); break;
                case ElementType::Mount:        color = glm::vec3(0.5f, 0.5f, 0.55f); break;
                case ElementType::ImportedMesh: color = glm::vec3(0.7f, 0.7f, 0.7f); break;
            }
        }

        if (elem->type == ElementType::ImportedMesh) {
            // Per-instance cache for imported meshes
            auto it = meshCache.find(elem->id);
            if (it == meshCache.end()) {
                auto inserted = meshCache.emplace(elem->id, createCachedMesh(elem->meshVertices));
                solidMesh = &inserted.first->second;
            } else {
                solidMesh = &it->second;
            }
        } else {
            solidMesh = &prototypeGeometry[typeIdx];
        }

        bool isSelected = !forExport && scene->isSelected(elem->id);
        if (isSelected) color = color * (style ? style->selectionBrightness : 1.3f);

        // In Presentation mode, defer transparent elements to second pass
        if (isPresentation && elem->material.transparency > 0.01f) {
            transparentElements.push_back({elem.get(), solidMesh, color, isSelected});
            // Still draw wireframe for schematic/selected
        } else {
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
            activeShader.setVec3("uColor", color);
            activeShader.setFloat("uAlpha", isSelected ? 1.0f : 0.9f);
            activeShader.setMat3("uNormalMatrix", normalMatrix);
            activeShader.setVec3("uLightPos", camera.position);
            activeShader.setVec3("uViewPos", camera.position);

            // Set material uniforms in Presentation mode
            if (isPresentation) {
                activeShader.setFloat("uMetallic", elem->material.metallic);
                activeShader.setFloat("uRoughness", elem->material.roughness);
                activeShader.setFloat("uTransparency", 0.0f); // opaque pass
                activeShader.setFloat("uFresnelIOR", elem->material.fresnelIOR);
            }

            if (solidMesh && solidMesh->vao != 0) {
                glBindVertexArray(solidMesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, solidMesh->vertexCount);
            }
        }

        // Wireframe overlay: Schematic draws on ALL elements (dark outlines);
        // Standard/Presentation only draw on selected elements (green).
        bool drawWireframe = false;
        glm::vec3 wireColor = style ? style->wireframeColor : glm::vec3(0.2f, 1.0f, 0.2f);
        if (isSchematic && !forExport && elem->type != ElementType::ImportedMesh) {
            drawWireframe = true;
            wireColor = glm::vec3(0.0f, 0.0f, 0.0f); // pure black outlines for schematic
        } else if (isSelected && !forExport && elem->type != ElementType::ImportedMesh) {
            drawWireframe = true;
        }

        if (drawWireframe) {
            CachedMesh& wf = prototypeWireframe[typeIdx];
            if (wf.vao != 0) {
                // Use gridShader for wireframe (simpler, no material needed)
                gridShader.use();
                gridShader.setMat4("uView", camera.getViewMatrix());
                gridShader.setMat4("uProjection", camera.getProjectionMatrix());
                gridShader.setMat4("uModel", model);
                gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
                gridShader.setVec3("uLightPos", camera.position);
                gridShader.setVec3("uViewPos", camera.position);
                glBindVertexArray(wf.vao);
                glLineWidth(isSchematic ? 2.2f : 1.4f);
                gridShader.setVec3("uColor", wireColor);
                gridShader.setFloat("uAlpha", 1.0f);
                glDrawArrays(GL_LINES, 0, wf.vertexCount);
                glLineWidth(1.0f);
                // Switch back to active shader
                activeShader.use();
            }
        }
    }

    // Second pass: render transparent elements with blending (Presentation mode)
    if (isPresentation && !transparentElements.empty()) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);

        for (const auto& td : transparentElements) {
            glm::mat4 model = td.elem->transform.getMatrix();
            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

            activeShader.setMat4("uModel", model);
            activeShader.setVec3("uColor", td.color);
            activeShader.setFloat("uAlpha", td.isSelected ? 1.0f : 0.9f);
            activeShader.setMat3("uNormalMatrix", normalMatrix);
            activeShader.setVec3("uLightPos", camera.position);
            activeShader.setVec3("uViewPos", camera.position);
            activeShader.setFloat("uMetallic", td.elem->material.metallic);
            activeShader.setFloat("uRoughness", td.elem->material.roughness);
            activeShader.setFloat("uTransparency", td.elem->material.transparency);
            activeShader.setFloat("uFresnelIOR", td.elem->material.fresnelIOR);

            if (td.mesh && td.mesh->vao != 0) {
                glBindVertexArray(td.mesh->vao);
                glDrawArrays(GL_TRIANGLES, 0, td.mesh->vertexCount);
            }
        }

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);

    // Re-enable face culling for subsequent passes
    glEnable(GL_CULL_FACE);
}

void Viewport::renderBeams(Scene* scene) {
    if (!scene) return;

    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);

    // Beams are self-luminous — use emissive mode to bypass lighting and tonemapping.
    // This is critical for Presentation mode bloom (HDR values must survive to the FBO).
    gridShader.setFloat("uEmissive", 1.0f);

    // Lazy-init reusable beam buffer
    if (beamBuffer.vao == 0) {
        glGenVertexArrays(1, &beamBuffer.vao);
        glGenBuffers(1, &beamBuffer.vbo);
        glBindVertexArray(beamBuffer.vao);
        glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(beamBuffer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);

    for (const auto& beam : scene->getBeams()) {
        if (!beam->visible) continue;

        bool isSelected = scene->isSelected(beam->id);
        glLineWidth(isSelected ? 4.0f : 2.0f);

        float vertices[12] = {
            beam->start.x, beam->start.y, beam->start.z, 0.0f, 0.0f, 1.0f,
            beam->end.x, beam->end.y, beam->end.z, 0.0f, 0.0f, 1.0f
        };

        {
            glm::vec3 beamColor = isSelected ? glm::vec3(1.0f, 1.0f, 1.0f) : beam->color;
            // Boost beam brightness in Presentation mode for bloom
            if (style && style->renderMode == RenderMode::Presentation) {
                beamColor *= 2.5f;
            }
            gridShader.setVec3("uColor", beamColor);
        }
        gridShader.setFloat("uAlpha", 1.0f);

        // Buffer orphaning: upload new data into existing buffer
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);
    gridShader.setFloat("uEmissive", 0.0f);
}

void Viewport::renderBeam(const Beam& beam) {
    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);
    gridShader.setFloat("uEmissive", 1.0f);
    gridShader.setVec3("uColor", beam.color);
    gridShader.setFloat("uAlpha", 0.7f);

    // Lazy-init reusable beam buffer
    if (beamBuffer.vao == 0) {
        glGenVertexArrays(1, &beamBuffer.vao);
        glGenBuffers(1, &beamBuffer.vbo);
        glBindVertexArray(beamBuffer.vao);
        glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    float vertices[12] = {
        beam.start.x, beam.start.y, beam.start.z, 0.0f, 0.0f, 1.0f,
        beam.end.x, beam.end.y, beam.end.z, 0.0f, 0.0f, 1.0f
    };

    glBindVertexArray(beamBuffer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
    glLineWidth(beam.width);
    glDrawArrays(GL_LINES, 0, 2);
    glLineWidth(1.0f);
    glBindVertexArray(0);
    gridShader.setFloat("uEmissive", 0.0f);
}

void Viewport::renderGaussianBeams(Scene* scene) {
    if (!scene) return;

    const int NUM_SAMPLES = 32;

    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);

    // Flat-shade the envelope: full ambient, no specular, so the color is
    // independent of viewing angle (the strip is a flat 2D billboard).
    gridShader.setFloat("uAmbientStrength", 1.0f);
    gridShader.setFloat("uSpecularStrength", 0.0f);

    // Lazy-init gaussian buffer
    if (gaussianBuffer.vao == 0) {
        glGenVertexArrays(1, &gaussianBuffer.vao);
        glGenBuffers(1, &gaussianBuffer.vbo);
        glBindVertexArray(gaussianBuffer.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gaussianBuffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, NUM_SAMPLES * 2 * 6 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    // Disable face culling (flat strip seen from both sides) and depth writes
    // (transparent overlay shouldn't block things behind it).
    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (const auto& bp : scene->getBeams()) {
        if (!bp || !bp->visible || !bp->isGaussian) continue;
        const Beam& beam = *bp;

        float beamLen = beam.getLength();  // in scene units (mm)
        if (beamLen < 1e-6f) continue;

        glm::vec3 dir = beam.getDirection();
        float waistZ = beam.waistPosition * beamLen; // mm

        // Perpendicular vector facing the camera (billboard).
        // Compute cross product BEFORE normalizing to safely detect degenerate case.
        glm::vec3 midpoint = (beam.start + beam.end) * 0.5f;
        glm::vec3 toCamera = camera.position - midpoint;
        glm::vec3 crossVec = glm::cross(dir, toCamera);
        if (glm::dot(crossVec, crossVec) < 1e-12f) {
            crossVec = glm::cross(dir, camera.up);
        }
        glm::vec3 perp = glm::normalize(crossVec);

        // Build triangle strip: for each sample, upper vertex then lower vertex.
        // beamRadiusAt() works in meters (w0 and wavelength are meters),
        // so convert z from mm to meters, then convert result back to mm.
        std::vector<float> verts;
        verts.reserve(NUM_SAMPLES * 2 * 6);

        for (int i = 0; i < NUM_SAMPLES; i++) {
            float t = static_cast<float>(i) / static_cast<float>(NUM_SAMPLES - 1);
            glm::vec3 pos = beam.start + t * (beam.end - beam.start);
            float zMM = std::abs(t * beamLen - waistZ);  // mm
            float zM  = zMM * 0.001f;                     // convert to meters
            float wMM = beam.beamRadiusAt(zM) * 1000.0f;  // result in meters -> mm

            glm::vec3 upper = pos + perp * wMM;
            glm::vec3 lower = pos - perp * wMM;
            // Normal faces toward camera for consistent lighting
            glm::vec3 normal = glm::normalize(toCamera);

            verts.push_back(upper.x); verts.push_back(upper.y); verts.push_back(upper.z);
            verts.push_back(normal.x); verts.push_back(normal.y); verts.push_back(normal.z);
            verts.push_back(lower.x); verts.push_back(lower.y); verts.push_back(lower.z);
            verts.push_back(normal.x); verts.push_back(normal.y); verts.push_back(normal.z);
        }

        gridShader.setVec3("uColor", beam.color);
        gridShader.setFloat("uAlpha", 0.3f);

        glBindVertexArray(gaussianBuffer.vao);
        glBindBuffer(GL_ARRAY_BUFFER, gaussianBuffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                     verts.data(), GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, NUM_SAMPLES * 2);
        glBindVertexArray(0);
    }

    // Restore state
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    // Restore lighting uniforms to scene style defaults
    if (style) {
        gridShader.setFloat("uAmbientStrength", style->ambientStrength);
        gridShader.setFloat("uSpecularStrength", style->specularStrength);
    }
}

void Viewport::renderFocalPoints(Scene* scene) {
    if (!scene || !style || !style->showFocalPoints) return;

    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);
    // Flat shading for markers
    gridShader.setFloat("uAmbientStrength", 1.0f);
    gridShader.setFloat("uSpecularStrength", 0.0f);

    glm::vec3 markerColor(1.0f, 0.6f, 0.1f); // Orange
    gridShader.setVec3("uColor", markerColor);
    gridShader.setFloat("uAlpha", 1.0f);

    // Lazy-init beam buffer (reuse for line drawing)
    if (beamBuffer.vao == 0) {
        glGenVertexArrays(1, &beamBuffer.vao);
        glGenBuffers(1, &beamBuffer.vbo);
        glBindVertexArray(beamBuffer.vao);
        glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, 48 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(beamBuffer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);
    glLineWidth(2.0f);

    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;
        if (elem->optics.opticalType != OpticalType::Lens) continue;

        float focalLen = elem->optics.focalLength;
        if (std::abs(focalLen) < 0.01f) continue;

        glm::mat4 model = elem->transform.getMatrix();
        glm::vec3 center = elem->getWorldBoundsCenter();
        glm::vec3 forward = glm::normalize(glm::vec3(model * glm::vec4(0, 0, 1, 0)));

        // Two focal points: +f and -f along the lens axis
        float markerSize = 1.5f; // mm half-size of the X marker

        for (int side = -1; side <= 1; side += 2) {
            glm::vec3 fp = center + forward * (focalLen * static_cast<float>(side));

            // X marker: two crossed lines. Use camera-facing perpendicular vectors.
            glm::vec3 toCamera = camera.position - fp;
            glm::vec3 right = glm::normalize(glm::cross(forward, toCamera));
            if (glm::length(right) < 0.001f) right = glm::vec3(1, 0, 0);
            glm::vec3 up = glm::normalize(glm::cross(right, forward));

            // Diagonal 1: top-right to bottom-left
            // Diagonal 2: top-left to bottom-right
            glm::vec3 n(0, 1, 0);
            float vertices[48] = {
                // Line 1
                fp.x + right.x * markerSize + up.x * markerSize,
                fp.y + right.y * markerSize + up.y * markerSize,
                fp.z + right.z * markerSize + up.z * markerSize,
                n.x, n.y, n.z,
                fp.x - right.x * markerSize - up.x * markerSize,
                fp.y - right.y * markerSize - up.y * markerSize,
                fp.z - right.z * markerSize - up.z * markerSize,
                n.x, n.y, n.z,
                // Line 2
                fp.x - right.x * markerSize + up.x * markerSize,
                fp.y - right.y * markerSize + up.y * markerSize,
                fp.z - right.z * markerSize + up.z * markerSize,
                n.x, n.y, n.z,
                fp.x + right.x * markerSize - up.x * markerSize,
                fp.y + right.y * markerSize - up.y * markerSize,
                fp.z + right.z * markerSize - up.z * markerSize,
                n.x, n.y, n.z,
            };

            glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_LINES, 0, 4);
        }
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);

    // Restore lighting
    if (style) {
        gridShader.setFloat("uAmbientStrength", style->ambientStrength);
        gridShader.setFloat("uSpecularStrength", style->specularStrength);
    }
}

void Viewport::renderGizmo(Scene* scene, GizmoType gizmoType, int hoveredHandle, int exclusiveHandle) {
    if (!gizmo || !scene) return;
    
    Element* selected = scene->getSelectedElement();
    if (!selected) return;
    
    gizmo->render(camera, selected, gizmoType, width, height, hoveredHandle, exclusiveHandle);
}

void Viewport::renderGizmoAt(const glm::vec3& center, GizmoType gizmoType, int hoveredHandle, int exclusiveHandle) {
    if (!gizmo) return;

    // Create a temporary element at the given center so we can reuse Gizmo::render
    Element tmp(ElementType::Laser, "__gizmo_tmp");
    tmp.transform.position = center;
    gizmo->render(camera, &tmp, gizmoType, width, height, hoveredHandle, exclusiveHandle);
}

int Viewport::getGizmoHoveredHandle(Element* selectedElement, GizmoType gizmoType,
                                    float viewportX, float viewportY) const {
    if (!gizmo || !selectedElement) return -1;
    return gizmo->getHoveredHandle(camera, selectedElement, gizmoType, viewportX, viewportY, width, height);
}

// --- Fullscreen Quad (shared by bloom and gradient) ---

void Viewport::initFullscreenQuad() {
    if (fullscreenVAO != 0) return;

    float quadVertices[] = {
        // positions    // texcoords
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,

        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };

    glGenVertexArrays(1, &fullscreenVAO);
    glGenBuffers(1, &fullscreenVBO);
    glBindVertexArray(fullscreenVAO);
    glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);
}

// --- HDRI Environment Map ---

void Viewport::loadHdriTexture(const std::string& path) {
    if (path == loadedHdriPath && hdriTexture != 0) return;
    destroyHdriTexture();
    stbi_set_flip_vertically_on_load(1);
    int w, h, c;
    float* data = stbi_loadf(path.c_str(), &w, &h, &c, 3);
    stbi_set_flip_vertically_on_load(0);
    if (!data) {
        std::cerr << "Failed to load HDRI: " << path << std::endl;
        return;
    }
    glGenTextures(1, &hdriTexture);
    glBindTexture(GL_TEXTURE_2D, hdriTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(data);
    loadedHdriPath = path;
}

void Viewport::destroyHdriTexture() {
    if (hdriTexture != 0) {
        glDeleteTextures(1, &hdriTexture);
        hdriTexture = 0;
    }
    loadedHdriPath.clear();
}

// --- Bloom ---

void Viewport::initBloom() {
    if (bloomInitialized) return;

    initFullscreenQuad();

    // Create ping-pong FBOs with GL_RGB16F textures
    for (int i = 0; i < 2; i++) {
        glGenFramebuffers(1, &bloomFBO[i]);
        glGenTextures(1, &bloomTexture[i]);
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[i]);
        glBindTexture(GL_TEXTURE_2D, bloomTexture[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bloomTexture[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Load bloom shaders (embedded fallback)
    const char* fsVert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;
out vec2 TexCoords;
void main() { TexCoords = aTexCoords; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

    const char* extractFrag = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D uScene;
uniform float uThreshold = 0.8;
void main() {
    vec3 color = texture(uScene, TexCoords).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
    if (brightness > uThreshold) FragColor = vec4(color * (brightness - uThreshold), 1.0);
    else FragColor = vec4(0.0, 0.0, 0.0, 1.0);
}
)";

    const char* blurFrag = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D uImage;
uniform bool uHorizontal;
const float weight[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
void main() {
    vec2 texOffset = 1.0 / textureSize(uImage, 0);
    vec3 result = texture(uImage, TexCoords).rgb * weight[0];
    if (uHorizontal) {
        for (int i = 1; i < 5; ++i) {
            result += texture(uImage, TexCoords + vec2(texOffset.x * i, 0.0)).rgb * weight[i];
            result += texture(uImage, TexCoords - vec2(texOffset.x * i, 0.0)).rgb * weight[i];
        }
    } else {
        for (int i = 1; i < 5; ++i) {
            result += texture(uImage, TexCoords + vec2(0.0, texOffset.y * i)).rgb * weight[i];
            result += texture(uImage, TexCoords - vec2(0.0, texOffset.y * i)).rgb * weight[i];
        }
    }
    FragColor = vec4(result, 1.0);
}
)";

    const char* compositeFrag = R"(
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomIntensity = 1.0;
void main() {
    vec3 sceneColor = texture(uScene, TexCoords).rgb;
    vec3 bloomColor = texture(uBloom, TexCoords).rgb;
    vec3 result = sceneColor + bloomColor * uBloomIntensity;
    result = result / (1.0 + result);
    FragColor = vec4(result, 1.0);
}
)";

    bloomExtractShader.loadFromSource(fsVert, extractFrag);
    bloomBlurShader.loadFromSource(fsVert, blurFrag);
    bloomCompositeShader.loadFromSource(fsVert, compositeFrag);

    bloomInitialized = true;
}

void Viewport::destroyBloom() {
    for (int i = 0; i < 2; i++) {
        if (bloomFBO[i]) { glDeleteFramebuffers(1, &bloomFBO[i]); bloomFBO[i] = 0; }
        if (bloomTexture[i]) { glDeleteTextures(1, &bloomTexture[i]); bloomTexture[i] = 0; }
    }
    if (fullscreenVAO) { glDeleteVertexArrays(1, &fullscreenVAO); fullscreenVAO = 0; }
    if (fullscreenVBO) { glDeleteBuffers(1, &fullscreenVBO); fullscreenVBO = 0; }
    bloomInitialized = false;
}

void Viewport::renderBeamHighlight(const glm::vec3& beamStart, const glm::vec3& beamEnd,
                                   const glm::vec3& snapPoint) {
    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);
    gridShader.setFloat("uEmissive", 1.0f);

    // Lazy-init reusable beam buffer
    if (beamBuffer.vao == 0) {
        glGenVertexArrays(1, &beamBuffer.vao);
        glGenBuffers(1, &beamBuffer.vbo);
        glBindVertexArray(beamBuffer.vao);
        glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);
        glBufferData(GL_ARRAY_BUFFER, 12 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(beamBuffer.vao);
    glBindBuffer(GL_ARRAY_BUFFER, beamBuffer.vbo);

    // Draw the beam line in cyan (highlighted)
    glm::vec3 cyan(0.0f, 1.0f, 1.0f);
    gridShader.setVec3("uColor", cyan);
    gridShader.setFloat("uAlpha", 0.8f);
    glLineWidth(4.0f);

    float lineVerts[12] = {
        beamStart.x, beamStart.y, beamStart.z, 0.0f, 1.0f, 0.0f,
        beamEnd.x, beamEnd.y, beamEnd.z, 0.0f, 1.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(lineVerts), lineVerts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, 2);

    // Draw yellow cross at snap point
    glm::vec3 yellow(1.0f, 1.0f, 0.0f);
    gridShader.setVec3("uColor", yellow);
    gridShader.setFloat("uAlpha", 1.0f);
    glLineWidth(3.0f);

    float crossSize = 2.0f;
    float crossVerts[24] = {
        snapPoint.x - crossSize, snapPoint.y, snapPoint.z, 0.0f, 1.0f, 0.0f,
        snapPoint.x + crossSize, snapPoint.y, snapPoint.z, 0.0f, 1.0f, 0.0f,
        snapPoint.x, snapPoint.y, snapPoint.z - crossSize, 0.0f, 1.0f, 0.0f,
        snapPoint.x, snapPoint.y, snapPoint.z + crossSize, 0.0f, 1.0f, 0.0f,
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(crossVerts), crossVerts, GL_DYNAMIC_DRAW);
    glDrawArrays(GL_LINES, 0, 4);

    glLineWidth(1.0f);
    glBindVertexArray(0);
    gridShader.setFloat("uEmissive", 0.0f);
}

void Viewport::renderBloomPass() {
    if (!style || style->renderMode != RenderMode::Presentation) return;
    if (!bloomInitialized) initBloom();

    // Resize bloom textures if needed
    GLint texW, texH;
    glBindTexture(GL_TEXTURE_2D, bloomTexture[0]);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texW);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texH);
    if (texW != width || texH != height) {
        for (int i = 0; i < 2; i++) {
            glBindTexture(GL_TEXTURE_2D, bloomTexture[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, width, height, 0, GL_RGB, GL_FLOAT, nullptr);
        }
    }

    glDisable(GL_DEPTH_TEST);

    // Step 1: Extract bright pixels from scene texture
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[0]);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    bloomExtractShader.use();
    bloomExtractShader.setFloat("uThreshold", style->bloomThreshold);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);  // scene texture
    bloomExtractShader.setInt("uScene", 0);
    glBindVertexArray(fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Step 2: Ping-pong Gaussian blur
    bool horizontal = true;
    bloomBlurShader.use();
    for (int i = 0; i < style->bloomBlurPasses * 2; i++) {
        int target = horizontal ? 1 : 0;
        int source = horizontal ? 0 : 1;
        glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[target]);
        glClear(GL_COLOR_BUFFER_BIT);
        bloomBlurShader.setBool("uHorizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bloomTexture[source]);
        bloomBlurShader.setInt("uImage", 0);
        glBindVertexArray(fullscreenVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
    }

    // Step 3: Composite bloom with scene
    // We can't read textureId while writing to framebufferId (same attachment).
    // So composite into bloomFBO[0], then blit back to the main FBO.
    int bloomResult = horizontal ? 0 : 1;

    // Use bloomFBO[0] as composite target (reusing it)
    int compositeTarget = (bloomResult == 0) ? 1 : 0;
    glBindFramebuffer(GL_FRAMEBUFFER, bloomFBO[compositeTarget]);
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT);
    bloomCompositeShader.use();
    bloomCompositeShader.setFloat("uBloomIntensity", style->bloomIntensity);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);  // original scene
    bloomCompositeShader.setInt("uScene", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomTexture[bloomResult]);
    bloomCompositeShader.setInt("uBloom", 1);
    glBindVertexArray(fullscreenVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Blit the composite result back to the main FBO
    glBindFramebuffer(GL_READ_FRAMEBUFFER, bloomFBO[compositeTarget]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebufferId);
    glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

// --- Thumbnail rendering for library panel ---

void Viewport::destroyThumbnails() {
    if (thumbnailFBO) { glDeleteFramebuffers(1, &thumbnailFBO); thumbnailFBO = 0; }
    if (thumbnailDepthRBO) { glDeleteRenderbuffers(1, &thumbnailDepthRBO); thumbnailDepthRBO = 0; }
    for (int i = 0; i < kMaxPrototypes; i++) {
        if (thumbnailTextures[i]) { glDeleteTextures(1, &thumbnailTextures[i]); thumbnailTextures[i] = 0; }
    }
    thumbnailsGenerated = false;
}

GLuint Viewport::getThumbnailTexture(int typeIndex) const {
    if (typeIndex < 0 || typeIndex >= kMaxPrototypes) return 0;
    return thumbnailTextures[typeIndex];
}

void Viewport::generateThumbnails() {
    if (!prototypesInitialized) initPrototypeGeometry();

    // Save current GL state
    GLint prevFBO = 0, prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // Create thumbnail FBO if needed
    if (!thumbnailFBO) {
        glGenFramebuffers(1, &thumbnailFBO);
        glGenRenderbuffers(1, &thumbnailDepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, thumbnailDepthRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kThumbnailSize, kThumbnailSize);
        glBindFramebuffer(GL_FRAMEBUFFER, thumbnailFBO);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, thumbnailDepthRBO);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // Default element colors (matching viewport.cpp fallback colors)
    glm::vec3 defaultColors[] = {
        {1.0f, 0.2f, 0.2f},  // Laser
        {0.7f, 0.7f, 0.8f},  // Mirror
        {0.4f, 0.7f, 1.0f},  // Lens
        {0.9f, 0.9f, 0.4f},  // BeamSplitter
        {0.2f, 0.9f, 0.2f},  // Detector
        {0.6f, 0.4f, 0.8f},  // Filter
        {0.8f, 0.6f, 0.3f},  // Aperture
        {0.5f, 0.8f, 0.9f},  // Prism
        {0.5f, 0.8f, 0.9f},  // PrismRA
        {0.7f, 0.5f, 0.3f},  // Grating
        {1.0f, 0.6f, 0.2f},  // FiberCoupler
        {0.3f, 0.8f, 0.3f},  // Screen
        {0.5f, 0.5f, 0.55f}, // Mount
        {0.7f, 0.7f, 0.7f},  // ImportedMesh
    };

    // Camera setup: 3/4 view angle
    glm::vec3 viewDir = glm::normalize(glm::vec3(1.0f, 0.7f, 1.2f));
    float distance = 3.0f;
    glm::vec3 eye = viewDir * distance;
    glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    int numTypes = (int)ElementType::ImportedMesh; // skip ImportedMesh

    for (int i = 0; i < numTypes; i++) {
        // Create output texture if needed
        if (!thumbnailTextures[i]) {
            glGenTextures(1, &thumbnailTextures[i]);
            glBindTexture(GL_TEXTURE_2D, thumbnailTextures[i]);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kThumbnailSize, kThumbnailSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        // Attach texture as color target
        glBindFramebuffer(GL_FRAMEBUFFER, thumbnailFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, thumbnailTextures[i], 0);
        glViewport(0, 0, kThumbnailSize, kThumbnailSize);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        // Fit orthographic projection to element
        CachedMesh& mesh = prototypeGeometry[i];
        if (mesh.vao == 0) continue;

        float extent = 0.8f; // default extent
        // Use larger extent for taller/wider elements
        if (i == (int)ElementType::Laser || i == (int)ElementType::Mount) extent = 1.0f;
        if (i == (int)ElementType::Screen) extent = 1.2f;

        glm::mat4 proj = glm::ortho(-extent, extent, -extent, extent, 0.1f, 20.0f);
        glm::mat4 model = glm::mat4(1.0f);

        // Use grid shader for consistent look
        gridShader.use();
        gridShader.setMat4("uModel", model);
        gridShader.setMat4("uView", view);
        gridShader.setMat4("uProjection", proj);
        gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));

        glm::vec3 color = (style && i < (int)ElementType::ImportedMesh)
            ? style->elementColors[i] : defaultColors[i];
        gridShader.setVec3("uColor", color);
        gridShader.setFloat("uAlpha", 1.0f);
        gridShader.setVec3("uLightPos", eye);
        gridShader.setVec3("uViewPos", eye);
        gridShader.setFloat("uAmbientStrength", 0.25f);
        gridShader.setFloat("uSpecularStrength", 0.5f);
        gridShader.setFloat("uShininess", 32.0f);
        gridShader.setFloat("uEmissive", 0.0f);

        glBindVertexArray(mesh.vao);
        glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
        glBindVertexArray(0);
    }

    // Restore GL state
    glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    thumbnailsGenerated = true;
}

// Helper: render scene to pixel buffer for export
static std::vector<unsigned char> renderForExport(Viewport& vp, Scene* scene) {
    vp.beginFrame();
    vp.renderScene(scene, true);
    vp.renderBeams(scene);
    vp.renderGaussianBeams(scene);
    std::vector<unsigned char> pixels(static_cast<size_t>(vp.getWidth()) * vp.getHeight() * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, vp.getWidth(), vp.getHeight(), GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    vp.endFrame();
    return pixels;
}

bool Viewport::exportToPng(const std::string& path, Scene* scene) {
    if (framebufferId == 0 || !scene) return false;
    auto pixels = renderForExport(*this, scene);
    return savePngToFile(path, width, height, pixels.data());
}

bool Viewport::exportToJpg(const std::string& path, Scene* scene, int quality) {
    if (framebufferId == 0 || !scene) return false;
    auto pixels = renderForExport(*this, scene);
    return saveJpgToFile(path, width, height, pixels.data(), quality);
}

bool Viewport::exportToPdf(const std::string& path, Scene* scene) {
    if (framebufferId == 0 || !scene) return false;
    auto pixels = renderForExport(*this, scene);
    return savePdfToFile(path, width, height, pixels.data());
}

} // namespace opticsketch
