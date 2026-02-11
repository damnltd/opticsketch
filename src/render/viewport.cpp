#include "render/viewport.h"
#include "render/gizmo.h"
#include "render/beam.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "export/export_png.h"
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
    for (int i = 0; i < 6; i++) {
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
void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(uViewPos - FragPos);
    
    // Key light
    vec3 lightDir = normalize(uLightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * vec3(1.0, 1.0, 1.0);
    
    // Fill light (opposite side) - gives roundness, less flat
    vec3 fillDir = normalize(-uLightPos - FragPos);
    float fillDiff = max(dot(norm, fillDir), 0.0);
    vec3 fillLight = fillDiff * vec3(0.4, 0.45, 0.5);
    
    // Ambient - low so shading is visible
    float ambientStrength = 0.14;
    vec3 ambient = ambientStrength * vec3(1.0, 1.0, 1.0);
    
    // Specular (Blinn-Phong)
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 48.0);
    float specularStrength = 0.55;
    vec3 specular = specularStrength * spec * vec3(1.0, 1.0, 1.0);
    
    vec3 result = (ambient + diffuse + fillLight + specular) * uColor;
    result = result / (1.0 + 0.15 * length(result));
    FragColor = vec4(result, uAlpha);
}
)";
        gridShader.loadFromSource(vertSource, fragSource);
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
    
    // Create texture
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
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
    
    // Clear
    glClearColor(0.05f, 0.05f, 0.06f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
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
    gridShader.setVec3("uColor", glm::vec3(0.3f, 0.3f, 0.35f));
    gridShader.setFloat("uAlpha", 0.5f);
    
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

void Viewport::initPrototypeGeometry() {
    if (prototypesInitialized) return;

    // Solid geometry for each built-in ElementType
    prototypeGeometry[(int)ElementType::Laser]        = createCachedMesh(generateCubeSolid(1.0f));
    prototypeGeometry[(int)ElementType::Mirror]       = createCachedMesh(generateSphereSolid(0.5f, 16));
    prototypeGeometry[(int)ElementType::Lens]         = createCachedMesh(generateTorusSolid(0.4f, 0.15f, 16, 8));
    prototypeGeometry[(int)ElementType::BeamSplitter] = createCachedMesh(generateCylinderSolid(0.4f, 0.8f, 16));
    prototypeGeometry[(int)ElementType::Detector]     = createCachedMesh(generatePlaneSolid(1.0f));

    // Wireframe geometry (needs pos+normal format for the shader)
    prototypeWireframe[(int)ElementType::Laser]        = createCachedMesh(wireframeLinesToVertices(generateCubeWireframe(1.0f)));
    prototypeWireframe[(int)ElementType::Mirror]       = createCachedMesh(wireframeLinesToVertices(generateSphereWireframe(0.5f, 16)));
    prototypeWireframe[(int)ElementType::Lens]         = createCachedMesh(wireframeLinesToVertices(generateTorusWireframe(0.4f, 0.15f, 16, 8)));
    prototypeWireframe[(int)ElementType::BeamSplitter] = createCachedMesh(wireframeLinesToVertices(generateCylinderWireframe(0.4f, 0.8f, 16)));
    prototypeWireframe[(int)ElementType::Detector]     = createCachedMesh(wireframeLinesToVertices(generatePlaneWireframeQuads(1.0f)));

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

    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());

    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;

        glm::mat4 model = elem->transform.getMatrix();
        gridShader.setMat4("uModel", model);

        // Determine color and which cached mesh to use
        glm::vec3 color;
        CachedMesh* solidMesh = nullptr;

        int typeIdx = static_cast<int>(elem->type);
        switch (elem->type) {
            case ElementType::Laser:        color = glm::vec3(1.0f, 0.2f, 0.2f); break;
            case ElementType::Mirror:       color = glm::vec3(0.8f, 0.8f, 0.9f); break;
            case ElementType::Lens:         color = glm::vec3(0.7f, 0.9f, 1.0f); break;
            case ElementType::BeamSplitter: color = glm::vec3(0.9f, 0.9f, 0.7f); break;
            case ElementType::Detector:     color = glm::vec3(0.2f, 1.0f, 0.2f); break;
            case ElementType::ImportedMesh: color = glm::vec3(0.7f, 0.7f, 0.7f); break;
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

        bool isSelected = !forExport && (scene->getSelectedElement() == elem.get());
        if (isSelected) color = color * 1.3f;

        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        gridShader.setVec3("uColor", color);
        gridShader.setFloat("uAlpha", isSelected ? 1.0f : 0.9f);
        gridShader.setMat3("uNormalMatrix", normalMatrix);
        gridShader.setVec3("uLightPos", camera.position);
        gridShader.setVec3("uViewPos", camera.position);

        if (solidMesh && solidMesh->vao != 0) {
            glBindVertexArray(solidMesh->vao);
            glDrawArrays(GL_TRIANGLES, 0, solidMesh->vertexCount);
        }

        // Wireframe overlay for selected elements
        if (isSelected && !forExport && elem->type != ElementType::ImportedMesh) {
            CachedMesh& wf = prototypeWireframe[typeIdx];
            if (wf.vao != 0) {
                glBindVertexArray(wf.vao);
                glLineWidth(1.4f);
                gridShader.setVec3("uColor", glm::vec3(0.2f, 1.0f, 0.2f));
                gridShader.setFloat("uAlpha", 1.0f);
                glDrawArrays(GL_LINES, 0, wf.vertexCount);
                glLineWidth(1.0f);
            }
        }
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void Viewport::renderBeams(Scene* scene, const Beam* selectedBeam) {
    if (!scene) return;

    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);

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

        bool isSelected = (selectedBeam == beam.get());
        glLineWidth(isSelected ? 4.0f : 2.0f);

        float vertices[12] = {
            beam->start.x, beam->start.y, beam->start.z, 0.0f, 0.0f, 1.0f,
            beam->end.x, beam->end.y, beam->end.z, 0.0f, 0.0f, 1.0f
        };

        gridShader.setVec3("uColor", isSelected ? glm::vec3(1.0f, 1.0f, 1.0f) : beam->color);
        gridShader.setFloat("uAlpha", 1.0f);

        // Buffer orphaning: upload new data into existing buffer
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }

    glBindVertexArray(0);
    glLineWidth(1.0f);
}

void Viewport::renderBeam(const Beam& beam) {
    gridShader.use();
    gridShader.setMat4("uView", camera.getViewMatrix());
    gridShader.setMat4("uProjection", camera.getProjectionMatrix());
    gridShader.setMat4("uModel", glm::mat4(1.0f));
    gridShader.setMat3("uNormalMatrix", glm::mat3(1.0f));
    gridShader.setVec3("uLightPos", camera.position);
    gridShader.setVec3("uViewPos", camera.position);
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
}

void Viewport::renderGizmo(Scene* scene, GizmoType gizmoType, int hoveredHandle, int exclusiveHandle) {
    if (!gizmo || !scene) return;
    
    Element* selected = scene->getSelectedElement();
    if (!selected) return;
    
    gizmo->render(camera, selected, gizmoType, width, height, hoveredHandle, exclusiveHandle);
}

int Viewport::getGizmoHoveredHandle(Element* selectedElement, GizmoType gizmoType,
                                    float viewportX, float viewportY) const {
    if (!gizmo || !selectedElement) return -1;
    return gizmo->getHoveredHandle(camera, selectedElement, gizmoType, viewportX, viewportY, width, height);
}

bool Viewport::exportToPng(const std::string& path, Scene* scene) {
    if (framebufferId == 0 || !scene) return false;
    beginFrame();
    renderScene(scene, true);
    renderBeams(scene);
    std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
    // Row alignment 1 so glReadPixels writes exactly width*3 bytes per row (no padding)
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);  // restore default
    endFrame();
    bool ok = savePngToFile(path, width, height, pixels.data());
    return ok;
}

} // namespace opticsketch
