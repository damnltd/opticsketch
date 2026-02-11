#include "render/gizmo.h"
#include "render/shader.h"
#include "render/raycast.h"
#include <cmath>
#include <algorithm>
#include <glm/gtc/matrix_inverse.hpp>

namespace opticsketch {

Gizmo::Gizmo() {
}

void Gizmo::init(Shader* shader) {
    gizmoShader = shader;
}

void Gizmo::render(const Camera& camera, const Element* element, GizmoType type,
                   int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle) {
    if (!element) return;
    
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    glm::vec3 position = element->getWorldBoundsCenter();
    // Gizmo vertices are built in world space; use identity model so we don't double-transform (shader does uModel * aPos)
    if (gizmoShader) gizmoShader->setMat4("uModel", glm::mat4(1.0f));
    switch (type) {
        case GizmoType::Move:
            renderMoveGizmo(camera, position, view, proj, viewportWidth, viewportHeight, hoveredHandle, exclusiveHandle);
            break;
        case GizmoType::Rotate:
            renderRotateGizmo(camera, position, view, proj, viewportWidth, viewportHeight, hoveredHandle, exclusiveHandle);
            break;
        case GizmoType::Scale:
            renderScaleGizmo(camera, position, view, proj, viewportWidth, viewportHeight, hoveredHandle, exclusiveHandle);
            break;
    }
}

// World-space half-width so that the line appears ~10px wide on screen (OpenGL core ignores glLineWidth > 1)
static float thickLineHalfWidthWorld(const glm::vec3& camPos, const glm::vec3& lineCenter, float fovDegrees, int viewportHeight) {
    float d = glm::length(camPos - lineCenter);
    if (d < 1e-5f) return 0.02f;
    float fovRad = glm::radians(fovDegrees);
    float halfWidth = (5.0f * d * std::tan(fovRad * 0.5f)) / static_cast<float>(viewportHeight > 0 ? viewportHeight : 1);
    return std::max(halfWidth, 0.005f);
}

void Gizmo::renderMoveGizmo(const Camera& camera, const glm::vec3& position,
                            const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle) {
    const float axisLength = 1.0f;
    float halfWidth = thickLineHalfWidthWorld(camera.position, position + glm::vec3(axisLength * 0.5f), camera.fov, viewportHeight);
    if (!gizmoShader) return;
    gizmoShader->use();
    gizmoShader->setMat4("uModel", glm::mat4(1.0f));
    gizmoShader->setMat4("uView", view);
    gizmoShader->setMat4("uProjection", proj);
    gizmoShader->setMat3("uNormalMatrix", glm::mat3(1.0f));
    gizmoShader->setVec3("uLightPos", camera.position);
    gizmoShader->setVec3("uViewPos", camera.position);
    gizmoShader->setFloat("uAlpha", 1.0f);
    int highlight = (exclusiveHandle >= 0) ? exclusiveHandle : hoveredHandle;
    auto axisColor = [highlight](int axis, float r, float g, float b) {
        if (highlight == axis) return glm::vec3(1.0f, 1.0f, 1.0f);
        return glm::vec3(r, g, b);
    };
    std::vector<float> quad;
    auto drawAxis = [&](int axis, const glm::vec3& end, float r, float g, float b) {
        if (exclusiveHandle >= 0 && axis != exclusiveHandle) return;
        makeThickLineQuad(camera.position, position, end, halfWidth, quad);
        gizmoShader->setVec3("uColor", axisColor(axis, r, g, b));
        renderSolid(quad);
        quad.clear();
    };
    drawAxis(0, position + glm::vec3(axisLength, 0, 0), 1.0f, 0.2f, 0.2f);
    drawAxis(1, position + glm::vec3(0, axisLength, 0), 0.2f, 1.0f, 0.2f);
    drawAxis(2, position + glm::vec3(0, 0, axisLength), 0.2f, 0.2f, 1.0f);
}

void Gizmo::renderRotateGizmo(const Camera& camera, const glm::vec3& position,
                              const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle) {
    const float radius = 0.8f;
    const int segments = 32;
    float halfWidth = thickLineHalfWidthWorld(camera.position, position, camera.fov, viewportHeight);
    if (!gizmoShader) return;
    gizmoShader->use();
    gizmoShader->setMat4("uModel", glm::mat4(1.0f));
    gizmoShader->setMat4("uView", view);
    gizmoShader->setMat4("uProjection", proj);
    gizmoShader->setMat3("uNormalMatrix", glm::mat3(1.0f));
    gizmoShader->setVec3("uLightPos", camera.position);
    gizmoShader->setVec3("uViewPos", camera.position);
    gizmoShader->setFloat("uAlpha", 1.0f);
    int highlight = (exclusiveHandle >= 0) ? exclusiveHandle : hoveredHandle;
    auto axisColor = [highlight](int axis, float r, float g, float b) {
        if (highlight == axis) return glm::vec3(1.0f, 1.0f, 1.0f);
        return glm::vec3(r, g, b);
    };
    auto addThickCircle = [&](int axis, const glm::vec3& normal, const glm::vec3& color) {
        if (exclusiveHandle >= 0 && axis != exclusiveHandle) return;
        glm::vec3 n = glm::normalize(normal);
        glm::vec3 up = std::abs(n.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(n, up));
        glm::vec3 fwd = glm::cross(right, n);
        std::vector<float> quad;
        for (int i = 0; i < segments; i++) {
            float a1 = 2.0f * 3.14159f * i / segments;
            float a2 = 2.0f * 3.14159f * (i + 1) / segments;
            glm::vec3 p1 = position + (right * std::cos(a1) + fwd * std::sin(a1)) * radius;
            glm::vec3 p2 = position + (right * std::cos(a2) + fwd * std::sin(a2)) * radius;
            makeThickLineQuad(camera.position, p1, p2, halfWidth, quad);
        }
        gizmoShader->setVec3("uColor", axisColor(axis, color.r, color.g, color.b));
        renderSolid(quad);
    };
    addThickCircle(0, glm::vec3(1, 0, 0), glm::vec3(1.0f, 0.2f, 0.2f));
    addThickCircle(1, glm::vec3(0, 1, 0), glm::vec3(0.2f, 1.0f, 0.2f));
    addThickCircle(2, glm::vec3(0, 0, 1), glm::vec3(0.2f, 0.2f, 1.0f));
}

void Gizmo::renderScaleGizmo(const Camera& camera, const glm::vec3& position,
                            const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle) {
    const float axisLength = 1.0f;
    float halfWidth = thickLineHalfWidthWorld(camera.position, position + glm::vec3(axisLength * 0.5f), camera.fov, viewportHeight);
    if (!gizmoShader) return;
    gizmoShader->use();
    gizmoShader->setMat4("uModel", glm::mat4(1.0f));
    gizmoShader->setMat4("uView", view);
    gizmoShader->setMat4("uProjection", proj);
    gizmoShader->setMat3("uNormalMatrix", glm::mat3(1.0f));
    gizmoShader->setVec3("uLightPos", camera.position);
    gizmoShader->setVec3("uViewPos", camera.position);
    gizmoShader->setFloat("uAlpha", 1.0f);
    int highlight = (exclusiveHandle >= 0) ? exclusiveHandle : hoveredHandle;
    auto axisColor = [highlight](int axis, float r, float g, float b) {
        if (highlight == axis) return glm::vec3(1.0f, 1.0f, 1.0f);
        return glm::vec3(r, g, b);
    };
    std::vector<float> quad;
    auto drawAxis = [&](int axis, const glm::vec3& end, float r, float g, float b) {
        if (exclusiveHandle >= 0 && axis != exclusiveHandle) return;
        makeThickLineQuad(camera.position, position, end, halfWidth, quad);
        gizmoShader->setVec3("uColor", axisColor(axis, r, g, b));
        renderSolid(quad);
        quad.clear();
    };
    drawAxis(0, position + glm::vec3(axisLength, 0, 0), 1.0f, 0.2f, 0.2f);
    drawAxis(1, position + glm::vec3(0, axisLength, 0), 0.2f, 1.0f, 0.2f);
    drawAxis(2, position + glm::vec3(0, 0, axisLength), 0.2f, 0.2f, 1.0f);
    // Extra white lines (Y–X, Y–Z, X–Z) only when not dragging a single axis
    if (exclusiveHandle < 0) {
        makeThickLineQuad(camera.position, position + glm::vec3(0, axisLength, 0), position + glm::vec3(axisLength, 0, 0), halfWidth, quad);
        gizmoShader->setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
        renderSolid(quad); quad.clear();
        makeThickLineQuad(camera.position, position + glm::vec3(0, axisLength, 0), position + glm::vec3(0, 0, axisLength), halfWidth, quad);
        gizmoShader->setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
        renderSolid(quad); quad.clear();
        makeThickLineQuad(camera.position, position + glm::vec3(axisLength, 0, 0), position + glm::vec3(0, 0, axisLength), halfWidth, quad);
        gizmoShader->setVec3("uColor", glm::vec3(1.0f, 1.0f, 1.0f));
        renderSolid(quad);
    }
}

void Gizmo::cleanup() {
    if (solidVAO != 0) {
        glDeleteVertexArrays(1, &solidVAO);
        glDeleteBuffers(1, &solidVBO);
        solidVAO = 0;
        solidVBO = 0;
    }
}

void Gizmo::renderSolid(const std::vector<float>& vertices) {
    if (vertices.size() < 18) return;

    // Lazy-init reusable VAO/VBO
    if (solidVAO == 0) {
        glGenVertexArrays(1, &solidVAO);
        glGenBuffers(1, &solidVBO);
        glBindVertexArray(solidVAO);
        glBindBuffer(GL_ARRAY_BUFFER, solidVBO);
        glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }

    glBindVertexArray(solidVAO);
    glBindBuffer(GL_ARRAY_BUFFER, solidVBO);
    // Buffer orphaning: upload new data each call
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 6));
}

void Gizmo::makeThickLineQuad(const glm::vec3& camPos, const glm::vec3& a, const glm::vec3& b, float halfWidth, std::vector<float>& outVertices) {
    glm::vec3 D = glm::normalize(b - a);
    glm::vec3 mid = (a + b) * 0.5f;
    glm::vec3 V = glm::normalize(camPos - mid);
    glm::vec3 R = glm::normalize(glm::cross(D, V));
    glm::vec3 v0 = a - R * halfWidth;
    glm::vec3 v1 = a + R * halfWidth;
    glm::vec3 v2 = b + R * halfWidth;
    glm::vec3 v3 = b - R * halfWidth;
    glm::vec3 n(0.0f, 1.0f, 0.0f);
    auto push = [&](const glm::vec3& p) {
        outVertices.insert(outVertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
    };
    push(v0); push(v1); push(v2);
    push(v0); push(v2); push(v3);
}

std::vector<float> Gizmo::generateCircleLineLoop(const glm::vec3& center, const glm::vec3& normal, float radius, int segments) {
    std::vector<float> vertices;
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 up = std::abs(n.y) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    glm::vec3 right = glm::normalize(glm::cross(n, up));
    glm::vec3 forward = glm::cross(right, n);
    const float dummyN[3] = {0.0f, 1.0f, 0.0f};
    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * 3.14159f * i / segments;
        float a2 = 2.0f * 3.14159f * (i + 1) / segments;
        glm::vec3 p1 = center + (right * std::cos(a1) + forward * std::sin(a1)) * radius;
        glm::vec3 p2 = center + (right * std::cos(a2) + forward * std::sin(a2)) * radius;
        vertices.insert(vertices.end(), {p1.x, p1.y, p1.z});
        vertices.insert(vertices.end(), dummyN, dummyN + 3);
        vertices.insert(vertices.end(), {p2.x, p2.y, p2.z});
        vertices.insert(vertices.end(), dummyN, dummyN + 3);
    }
    return vertices;
}

void Gizmo::getMoveAxis(int handle, glm::vec3& outAxisDir, float& outLength) {
    outLength = 1.0f;
    switch (handle) {
        case 0: outAxisDir = glm::vec3(1, 0, 0); break;
        case 1: outAxisDir = glm::vec3(0, 1, 0); break;
        case 2: outAxisDir = glm::vec3(0, 0, 1); break;
        default: outAxisDir = glm::vec3(1, 0, 0); break;
    }
}

void Gizmo::getRotateAxis(int handle, glm::vec3& outAxisDir, float& outRadius) {
    outRadius = 0.8f;
    switch (handle) {
        case 0: outAxisDir = glm::vec3(1, 0, 0); break;
        case 1: outAxisDir = glm::vec3(0, 1, 0); break;
        case 2: outAxisDir = glm::vec3(0, 0, 1); break;
        default: outAxisDir = glm::vec3(1, 0, 0); break;
    }
}

int Gizmo::getHoveredHandle(const Camera& camera, const Element* element, GizmoType type,
                           float viewportX, float viewportY, int viewportWidth, int viewportHeight) {
    if (!element) return -1;
    
    Raycast::Ray ray = Raycast::screenToRay(camera, viewportX, viewportY, viewportWidth, viewportHeight);
    const glm::vec3 pos = element->getWorldBoundsCenter();
    
    const float pickThresholdSq = 0.022f;  // ~0.15 world units
    const float rotateRadius = 0.8f;
    const float rotateTolerance = 0.12f;   // within ring thickness
    
    if (type == GizmoType::Move || type == GizmoType::Scale) {
        const float axisLen = 1.0f;
        glm::vec3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
        float bestSq = pickThresholdSq;
        int best = -1;
        for (int i = 0; i < 3; i++) {
            glm::vec3 end = pos + axes[i] * axisLen;
            float tRay, tSeg;
            float sqDist = Raycast::rayToSegmentSqDist(ray, pos, end, tRay, tSeg);
            if (tRay >= 0.0f && sqDist < bestSq) {
                bestSq = sqDist;
                best = i;
            }
        }
        return best;
    }
    
    if (type == GizmoType::Rotate) {
        glm::vec3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
        float bestScore = rotateTolerance;
        int best = -1;
        for (int i = 0; i < 3; i++) {
            float t;
            if (!Raycast::intersectPlane(ray, pos, axes[i], t)) continue;
            if (t < 0.0f) continue;
            glm::vec3 hit = ray.origin + t * ray.direction;
            float distFromCenter = glm::length(hit - pos);
            float score = std::abs(distFromCenter - rotateRadius);
            if (score < bestScore) {
                bestScore = score;
                best = i;
            }
        }
        return best;
    }
    
    return -1;
}

} // namespace opticsketch
