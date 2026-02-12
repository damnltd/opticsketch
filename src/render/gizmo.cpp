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
                   int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle,
                   const glm::mat3& orientation, float dragAngle, float dragStartAngle) {
    if (!element) return;

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    glm::vec3 position = element->getWorldBoundsCenter();
    // Gizmo vertices are built in world space; use identity model so we don't double-transform (shader does uModel * aPos)
    if (gizmoShader) gizmoShader->setMat4("uModel", glm::mat4(1.0f));

    // Disable face culling so solid geometry (cones, cubes, plane squares) is visible
    // from all angles regardless of winding order
    glDisable(GL_CULL_FACE);

    // Enable blending for semi-transparent planar handles and arc feedback
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    switch (type) {
        case GizmoType::Move:
            renderMoveGizmo(camera, position, view, proj, viewportWidth, viewportHeight, hoveredHandle, exclusiveHandle, orientation);
            break;
        case GizmoType::Rotate:
            renderRotateGizmo(camera, position, view, proj, viewportWidth, viewportHeight, hoveredHandle, exclusiveHandle, orientation, dragAngle, dragStartAngle);
            break;
        case GizmoType::Scale:
            renderScaleGizmo(camera, position, view, proj, viewportWidth, viewportHeight, hoveredHandle, exclusiveHandle, orientation);
            break;
    }

    glDisable(GL_BLEND);
    glEnable(GL_CULL_FACE);  // Restore culling for subsequent rendering
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
                            const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle, const glm::mat3& orientation) {
    const float axisLength = 1.0f;
    const float coneLength = 0.15f;
    const float coneRadius = 0.06f;
    const float lineLength = axisLength - coneLength;
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

    glm::vec3 axes[3] = { orientation * glm::vec3(1,0,0), orientation * glm::vec3(0,1,0), orientation * glm::vec3(0,0,1) };
    glm::vec3 axisColors[3] = { {0.9f, 0.2f, 0.2f}, {0.5f, 0.9f, 0.2f}, {0.2f, 0.4f, 0.95f} };
    glm::vec3 planeColors[3] = { {0.9f, 0.9f, 0.2f}, {0.2f, 0.9f, 0.9f}, {0.9f, 0.2f, 0.9f} }; // XY, YZ, XZ
    glm::vec3 highlightColor(1.0f, 0.95f, 0.5f);
    glm::vec3 activeColor(1.0f, 1.0f, 0.3f);

    int highlight = (exclusiveHandle >= 0) ? exclusiveHandle : hoveredHandle;
    auto getColor = [&](int handle, const glm::vec3& baseColor) -> glm::vec3 {
        if (exclusiveHandle >= 0 && handle == exclusiveHandle) return activeColor;
        if (handle == highlight) return highlightColor;
        return baseColor;
    };

    // Determine which handles to show during exclusive drag
    auto shouldDraw = [&](int handle) -> bool {
        if (exclusiveHandle < 0) return true; // not dragging, show all
        if (handle == exclusiveHandle) return true;
        // For planar handles, also show the two relevant axis lines
        if (exclusiveHandle == GizmoHandle::XY) return (handle == GizmoHandle::X || handle == GizmoHandle::Y);
        if (exclusiveHandle == GizmoHandle::YZ) return (handle == GizmoHandle::Y || handle == GizmoHandle::Z);
        if (exclusiveHandle == GizmoHandle::XZ) return (handle == GizmoHandle::X || handle == GizmoHandle::Z);
        if (exclusiveHandle == GizmoHandle::Center) return (handle >= GizmoHandle::X && handle <= GizmoHandle::Z);
        return false;
    };

    std::vector<float> verts;

    // Draw axis lines + cone tips
    for (int i = 0; i < 3; i++) {
        if (!shouldDraw(i)) continue;
        glm::vec3 lineEnd = position + axes[i] * lineLength;
        glm::vec3 tipPos = position + axes[i] * axisLength;
        // Axis line
        makeThickLineQuad(camera.position, position, lineEnd, halfWidth, verts);
        gizmoShader->setVec3("uColor", getColor(i, axisColors[i]));
        renderSolid(verts);
        verts.clear();
        // Cone tip
        makeConeTip(tipPos, axes[i], coneLength, coneRadius, 8, verts);
        gizmoShader->setVec3("uColor", getColor(i, axisColors[i]));
        renderSolid(verts);
        verts.clear();
    }

    // Draw planar handles (small squares at 30% along each axis pair)
    const float plOffset = 0.2f;
    const float plSize = 0.15f;
    int planeHandles[3] = { GizmoHandle::XY, GizmoHandle::YZ, GizmoHandle::XZ };
    int planeAxis1[3] = { 0, 1, 0 }; // axis indices for each plane
    int planeAxis2[3] = { 1, 2, 2 };
    for (int p = 0; p < 3; p++) {
        if (!shouldDraw(planeHandles[p])) continue;
        bool isHighlighted = (highlight == planeHandles[p]);
        gizmoShader->setFloat("uAlpha", isHighlighted ? 0.7f : 0.35f);
        makePlaneSquare(position, axes[planeAxis1[p]], axes[planeAxis2[p]], plOffset, plSize,
                        glm::cross(axes[planeAxis1[p]], axes[planeAxis2[p]]), verts);
        gizmoShader->setVec3("uColor", getColor(planeHandles[p], planeColors[p]));
        renderSolid(verts);
        verts.clear();
        gizmoShader->setFloat("uAlpha", 1.0f);
        // Edge lines on two sides of the square
        glm::vec3 c0 = position + axes[planeAxis1[p]] * plOffset;
        glm::vec3 c1 = position + axes[planeAxis1[p]] * (plOffset + plSize);
        glm::vec3 c2 = position + axes[planeAxis2[p]] * plOffset;
        glm::vec3 c3 = position + axes[planeAxis2[p]] * (plOffset + plSize);
        makeThickLineQuad(camera.position, c0, c0 + axes[planeAxis2[p]] * plSize, halfWidth * 0.5f, verts);
        makeThickLineQuad(camera.position, c2, c2 + axes[planeAxis1[p]] * plSize, halfWidth * 0.5f, verts);
        gizmoShader->setVec3("uColor", getColor(planeHandles[p], planeColors[p]));
        renderSolid(verts);
        verts.clear();
    }

    // Center handle (small cube)
    if (shouldDraw(GizmoHandle::Center)) {
        makeCubeTip(position, 0.06f, verts);
        gizmoShader->setVec3("uColor", getColor(GizmoHandle::Center, glm::vec3(0.85f)));
        renderSolid(verts);
        verts.clear();
    }
}

void Gizmo::renderRotateGizmo(const Camera& camera, const glm::vec3& position,
                              const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle, const glm::mat3& orientation, float dragAngle, float dragStartAngle) {
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

    glm::vec3 axes[3] = { orientation * glm::vec3(1,0,0), orientation * glm::vec3(0,1,0), orientation * glm::vec3(0,0,1) };
    glm::vec3 axisColors[3] = { {0.9f, 0.2f, 0.2f}, {0.5f, 0.9f, 0.2f}, {0.2f, 0.4f, 0.95f} };
    glm::vec3 highlightColor(1.0f, 0.95f, 0.5f);
    glm::vec3 activeColor(1.0f, 1.0f, 0.3f);

    int highlight = (exclusiveHandle >= 0) ? exclusiveHandle : hoveredHandle;

    auto addThickCircle = [&](int axis, const glm::vec3& normal, const glm::vec3& color) {
        if (exclusiveHandle >= 0 && axis != exclusiveHandle) return;
        glm::vec3 n = glm::normalize(normal);
        glm::vec3 up = std::abs(glm::dot(n, glm::vec3(0,1,0))) < 0.9f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 right = glm::normalize(glm::cross(n, up));
        glm::vec3 fwd = glm::cross(right, n);
        std::vector<float> quad;
        for (int i = 0; i < segments; i++) {
            float a1 = 2.0f * 3.14159265f * i / segments;
            float a2 = 2.0f * 3.14159265f * (i + 1) / segments;
            glm::vec3 p1 = position + (right * std::cos(a1) + fwd * std::sin(a1)) * radius;
            glm::vec3 p2 = position + (right * std::cos(a2) + fwd * std::sin(a2)) * radius;
            makeThickLineQuad(camera.position, p1, p2, halfWidth, quad);
        }
        glm::vec3 c = color;
        if (exclusiveHandle >= 0 && axis == exclusiveHandle) c = activeColor;
        else if (axis == highlight) c = highlightColor;
        gizmoShader->setVec3("uColor", c);
        renderSolid(quad);
    };

    for (int i = 0; i < 3; i++) {
        addThickCircle(i, axes[i], axisColors[i]);
    }

    // Rotation arc feedback during drag
    if (exclusiveHandle >= 0 && std::abs(dragAngle) > 0.001f) {
        std::vector<float> arcVerts;
        makeArc(camera.position, position, axes[exclusiveHandle], dragStartAngle, dragAngle, radius, halfWidth * 1.5f, 32, arcVerts);
        gizmoShader->setFloat("uAlpha", 0.5f);
        gizmoShader->setVec3("uColor", axisColors[exclusiveHandle]);
        renderSolid(arcVerts);
        gizmoShader->setFloat("uAlpha", 1.0f);
    }
}

void Gizmo::renderScaleGizmo(const Camera& camera, const glm::vec3& position,
                            const glm::mat4& view, const glm::mat4& proj, int viewportWidth, int viewportHeight, int hoveredHandle, int exclusiveHandle, const glm::mat3& orientation) {
    const float axisLength = 1.0f;
    const float cubeHalf = 0.05f;
    const float lineLength = axisLength - cubeHalf;
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

    glm::vec3 axes[3] = { orientation * glm::vec3(1,0,0), orientation * glm::vec3(0,1,0), orientation * glm::vec3(0,0,1) };
    glm::vec3 axisColors[3] = { {0.9f, 0.2f, 0.2f}, {0.5f, 0.9f, 0.2f}, {0.2f, 0.4f, 0.95f} };
    glm::vec3 highlightColor(1.0f, 0.95f, 0.5f);
    glm::vec3 activeColor(1.0f, 1.0f, 0.3f);

    int highlight = (exclusiveHandle >= 0) ? exclusiveHandle : hoveredHandle;
    auto getColor = [&](int handle, const glm::vec3& baseColor) -> glm::vec3 {
        if (exclusiveHandle >= 0 && handle == exclusiveHandle) return activeColor;
        if (handle == highlight) return highlightColor;
        return baseColor;
    };

    auto shouldDraw = [&](int handle) -> bool {
        if (exclusiveHandle < 0) return true;
        if (handle == exclusiveHandle) return true;
        if (exclusiveHandle == GizmoHandle::Center) return (handle >= GizmoHandle::X && handle <= GizmoHandle::Z);
        return false;
    };

    std::vector<float> verts;

    // Draw axis lines + cube tips
    for (int i = 0; i < 3; i++) {
        if (!shouldDraw(i)) continue;
        glm::vec3 lineEnd = position + axes[i] * lineLength;
        glm::vec3 cubePos = position + axes[i] * axisLength;
        // Axis line
        makeThickLineQuad(camera.position, position, lineEnd, halfWidth, verts);
        gizmoShader->setVec3("uColor", getColor(i, axisColors[i]));
        renderSolid(verts);
        verts.clear();
        // Cube tip
        makeCubeTip(cubePos, cubeHalf, verts);
        gizmoShader->setVec3("uColor", getColor(i, axisColors[i]));
        renderSolid(verts);
        verts.clear();
    }

    // Center cube (slightly larger, for uniform scale)
    if (shouldDraw(GizmoHandle::Center)) {
        makeCubeTip(position, 0.06f, verts);
        gizmoShader->setVec3("uColor", getColor(GizmoHandle::Center, glm::vec3(0.85f)));
        renderSolid(verts);
        verts.clear();
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

void Gizmo::makeConeTip(const glm::vec3& tipPos, const glm::vec3& axisDir, float coneLength, float coneRadius, int segments, std::vector<float>& outVertices) {
    glm::vec3 dir = glm::normalize(axisDir);
    glm::vec3 baseCenter = tipPos - dir * coneLength;

    // Build a local coordinate frame perpendicular to axis
    glm::vec3 up = std::abs(glm::dot(dir, glm::vec3(0,1,0))) < 0.9f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 right = glm::normalize(glm::cross(dir, up));
    glm::vec3 fwd = glm::cross(right, dir);

    auto push = [&](const glm::vec3& p, const glm::vec3& n) {
        outVertices.insert(outVertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
    };

    for (int i = 0; i < segments; i++) {
        float a1 = 2.0f * 3.14159265f * i / segments;
        float a2 = 2.0f * 3.14159265f * (i + 1) / segments;
        glm::vec3 p1 = baseCenter + (right * std::cos(a1) + fwd * std::sin(a1)) * coneRadius;
        glm::vec3 p2 = baseCenter + (right * std::cos(a2) + fwd * std::sin(a2)) * coneRadius;

        // Side face normal: outward from cone surface
        // cross(tipPos-p1, p2-p1) points outward; winding p1→tipPos→p2 is CCW from outside
        glm::vec3 faceNormal = glm::normalize(glm::cross(tipPos - p1, p2 - p1));

        // Side triangle: p1, tipPos, p2 (CCW when viewed from outside)
        push(p1, faceNormal);
        push(tipPos, faceNormal);
        push(p2, faceNormal);

        // Base triangle: baseCenter, p1, p2 (facing -dir)
        push(baseCenter, -dir);
        push(p1, -dir);
        push(p2, -dir);
    }
}

void Gizmo::makeCubeTip(const glm::vec3& center, float halfSize, std::vector<float>& outVertices) {
    float h = halfSize;
    glm::vec3 c = center;

    // 8 corners
    glm::vec3 v[8] = {
        c + glm::vec3(-h, -h, -h), c + glm::vec3( h, -h, -h),
        c + glm::vec3( h,  h, -h), c + glm::vec3(-h,  h, -h),
        c + glm::vec3(-h, -h,  h), c + glm::vec3( h, -h,  h),
        c + glm::vec3( h,  h,  h), c + glm::vec3(-h,  h,  h)
    };

    auto push = [&](const glm::vec3& p, const glm::vec3& n) {
        outVertices.insert(outVertices.end(), {p.x, p.y, p.z, n.x, n.y, n.z});
    };

    // 6 faces, each 2 triangles (CCW winding from outside)
    struct Face { int a, b, c, d; glm::vec3 n; };
    Face faces[6] = {
        {0, 3, 2, 1, { 0, 0,-1}}, // -Z face
        {4, 5, 6, 7, { 0, 0, 1}}, // +Z face
        {0, 4, 7, 3, {-1, 0, 0}}, // -X face
        {5, 1, 2, 6, { 1, 0, 0}}, // +X face
        {3, 7, 6, 2, { 0, 1, 0}}, // +Y face
        {0, 1, 5, 4, { 0,-1, 0}}, // -Y face
    };
    for (auto& f : faces) {
        push(v[f.a], f.n); push(v[f.b], f.n); push(v[f.c], f.n);
        push(v[f.a], f.n); push(v[f.c], f.n); push(v[f.d], f.n);
    }
}

void Gizmo::makePlaneSquare(const glm::vec3& position, const glm::vec3& axis1, const glm::vec3& axis2, float offset, float size, const glm::vec3& normal, std::vector<float>& outVertices) {
    glm::vec3 n = glm::normalize(normal);
    glm::vec3 c0 = position + axis1 * offset + axis2 * offset;
    glm::vec3 c1 = position + axis1 * (offset + size) + axis2 * offset;
    glm::vec3 c2 = position + axis1 * (offset + size) + axis2 * (offset + size);
    glm::vec3 c3 = position + axis1 * offset + axis2 * (offset + size);

    auto push = [&](const glm::vec3& p, const glm::vec3& fn) {
        outVertices.insert(outVertices.end(), {p.x, p.y, p.z, fn.x, fn.y, fn.z});
    };
    // Front face
    push(c0, n); push(c1, n); push(c2, n);
    push(c0, n); push(c2, n); push(c3, n);
    // Back face (so it's visible from both sides)
    push(c0, -n); push(c2, -n); push(c1, -n);
    push(c0, -n); push(c3, -n); push(c2, -n);
}

void Gizmo::makeArc(const glm::vec3& camPos, const glm::vec3& center, const glm::vec3& axisDir, float startAngle, float sweepAngle, float radius, float halfWidth, int segments, std::vector<float>& outVertices) {
    glm::vec3 n = glm::normalize(axisDir);
    glm::vec3 up = std::abs(glm::dot(n, glm::vec3(0,1,0))) < 0.9f ? glm::vec3(0,1,0) : glm::vec3(1,0,0);
    glm::vec3 right = glm::normalize(glm::cross(n, up));
    glm::vec3 fwd = glm::cross(right, n);

    int numSegs = std::max(4, static_cast<int>(std::abs(sweepAngle) / (2.0f * 3.14159265f) * segments));
    float step = sweepAngle / numSegs;

    for (int i = 0; i < numSegs; i++) {
        float a1 = startAngle + step * i;
        float a2 = startAngle + step * (i + 1);
        glm::vec3 p1 = center + (right * std::cos(a1) + fwd * std::sin(a1)) * radius;
        glm::vec3 p2 = center + (right * std::cos(a2) + fwd * std::sin(a2)) * radius;
        makeThickLineQuad(camPos, p1, p2, halfWidth, outVertices);
    }

    // Filled wedge (semi-transparent fan from center)
    auto push = [&](const glm::vec3& p, const glm::vec3& fn) {
        outVertices.insert(outVertices.end(), {p.x, p.y, p.z, fn.x, fn.y, fn.z});
    };
    for (int i = 0; i < numSegs; i++) {
        float a1 = startAngle + step * i;
        float a2 = startAngle + step * (i + 1);
        glm::vec3 p1 = center + (right * std::cos(a1) + fwd * std::sin(a1)) * radius;
        glm::vec3 p2 = center + (right * std::cos(a2) + fwd * std::sin(a2)) * radius;
        push(center, n); push(p1, n); push(p2, n);
    }
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
                           float viewportX, float viewportY, int viewportWidth, int viewportHeight,
                           const glm::mat3& orientation) {
    if (!element) return -1;

    Raycast::Ray ray = Raycast::screenToRay(camera, viewportX, viewportY, viewportWidth, viewportHeight);
    const glm::vec3 pos = element->getWorldBoundsCenter();

    const float pickThresholdSq = 0.022f;  // ~0.15 world units
    const float rotateRadius = 0.8f;
    const float rotateTolerance = 0.12f;
    glm::vec3 axes[3] = { orientation * glm::vec3(1,0,0), orientation * glm::vec3(0,1,0), orientation * glm::vec3(0,0,1) };

    if (type == GizmoType::Move) {
        const float axisLen = 1.0f;
        const float centerHalf = 0.08f;
        const float plOffset = 0.2f;
        const float plSize = 0.15f;

        // 1) Test center cube first (highest priority, smallest target)
        {
            float t;
            glm::vec3 bmin = pos - glm::vec3(centerHalf);
            glm::vec3 bmax = pos + glm::vec3(centerHalf);
            if (Raycast::intersectAABB(ray, bmin, bmax, t) && t >= 0.0f) {
                return GizmoHandle::Center;
            }
        }

        // 2) Test planar handles (smaller targets, priority over axes)
        int planeHandleIds[3] = { GizmoHandle::XY, GizmoHandle::YZ, GizmoHandle::XZ };
        int plA1[3] = {0, 1, 0}; // axis indices
        int plA2[3] = {1, 2, 2};
        for (int p = 0; p < 3; p++) {
            glm::vec3 planeNormal = glm::cross(axes[plA1[p]], axes[plA2[p]]);
            if (glm::length(planeNormal) < 1e-6f) continue;
            planeNormal = glm::normalize(planeNormal);
            float t;
            if (!Raycast::intersectPlane(ray, pos, planeNormal, t)) continue;
            if (t < 0.0f) continue;
            glm::vec3 hit = ray.origin + t * ray.direction;
            glm::vec3 rel = hit - pos;
            float u = glm::dot(rel, axes[plA1[p]]);
            float v = glm::dot(rel, axes[plA2[p]]);
            if (u >= plOffset && u <= plOffset + plSize && v >= plOffset && v <= plOffset + plSize) {
                return planeHandleIds[p];
            }
        }

        // 3) Test axis lines (including cone tip region)
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

    if (type == GizmoType::Scale) {
        const float axisLen = 1.0f;
        const float centerHalf = 0.08f;

        // 1) Test center cube
        {
            float t;
            glm::vec3 bmin = pos - glm::vec3(centerHalf);
            glm::vec3 bmax = pos + glm::vec3(centerHalf);
            if (Raycast::intersectAABB(ray, bmin, bmax, t) && t >= 0.0f) {
                return GizmoHandle::Center;
            }
        }

        // 2) Test axis lines + cube tips
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
