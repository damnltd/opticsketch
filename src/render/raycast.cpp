#include "render/raycast.h"
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>

namespace opticsketch {

Raycast::Ray Raycast::screenToRay(const Camera& camera, float screenX, float screenY,
                                  int viewportWidth, int viewportHeight) {
    // Normalize screen coordinates to [-1, 1]
    float x = (2.0f * screenX) / viewportWidth - 1.0f;
    float y = 1.0f - (2.0f * screenY) / viewportHeight; // Flip Y
    
    // Get view and projection matrices
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    
    // Create ray in clip space
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    
    // Transform to eye space
    glm::mat4 invProj = glm::inverse(proj);
    glm::vec4 rayEye = invProj * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
    
    // Transform to world space
    glm::mat4 invView = glm::inverse(view);
    glm::vec4 rayWorld = invView * rayEye;
    
    Ray ray;
    ray.origin = glm::vec3(invView[3]); // Camera position
    ray.direction = glm::normalize(glm::vec3(rayWorld));
    
    return ray;
}

bool Raycast::intersectAABB(const Ray& ray, const glm::vec3& min, const glm::vec3& max, float& t) {
    glm::vec3 invDir = 1.0f / ray.direction;
    glm::vec3 t1 = (min - ray.origin) * invDir;
    glm::vec3 t2 = (max - ray.origin) * invDir;
    
    glm::vec3 tMin = glm::min(t1, t2);
    glm::vec3 tMax = glm::max(t1, t2);
    
    float tNear = std::max(std::max(tMin.x, tMin.y), tMin.z);
    float tFar = std::min(std::min(tMax.x, tMax.y), tMax.z);
    
    if (tNear > tFar || tFar < 0.0f) {
        return false;
    }
    
    t = (tNear > 0.0f) ? tNear : tFar;
    return true;
}

bool Raycast::intersectAABBWithNormal(const Ray& ray, const glm::vec3& min, const glm::vec3& max,
                                      float& t, glm::vec3& faceNormal) {
    glm::vec3 invDir = 1.0f / ray.direction;
    glm::vec3 t1 = (min - ray.origin) * invDir;
    glm::vec3 t2 = (max - ray.origin) * invDir;

    glm::vec3 tMin = glm::min(t1, t2);
    glm::vec3 tMax = glm::max(t1, t2);

    float tNear = std::max(std::max(tMin.x, tMin.y), tMin.z);
    float tFar = std::min(std::min(tMax.x, tMax.y), tMax.z);

    if (tNear > tFar || tFar < 0.0f) {
        return false;
    }

    t = (tNear > 0.0f) ? tNear : tFar;

    // Determine which face was hit based on which component produced tNear/tFar
    float hitT = t;
    faceNormal = glm::vec3(0.0f);
    if (hitT == tNear) {
        if (tNear == tMin.x) faceNormal = glm::vec3(ray.direction.x > 0 ? -1.0f : 1.0f, 0, 0);
        else if (tNear == tMin.y) faceNormal = glm::vec3(0, ray.direction.y > 0 ? -1.0f : 1.0f, 0);
        else faceNormal = glm::vec3(0, 0, ray.direction.z > 0 ? -1.0f : 1.0f);
    } else {
        // Hit from inside (tNear < 0, using tFar)
        if (hitT == tMax.x) faceNormal = glm::vec3(ray.direction.x > 0 ? 1.0f : -1.0f, 0, 0);
        else if (hitT == tMax.y) faceNormal = glm::vec3(0, ray.direction.y > 0 ? 1.0f : -1.0f, 0);
        else faceNormal = glm::vec3(0, 0, ray.direction.z > 0 ? 1.0f : -1.0f);
    }

    return true;
}

bool Raycast::intersectElement(const Ray& ray, const Element* element,
                               glm::mat4 transform, float& t) {
    if (!element) return false;
    
    // Transform ray to element's local space
    glm::mat4 invTransform = glm::inverse(transform);
    glm::vec3 localOrigin = glm::vec3(invTransform * glm::vec4(ray.origin, 1.0f));
    glm::vec3 localDir = glm::normalize(glm::vec3(invTransform * glm::vec4(ray.direction, 0.0f)));
    
    Ray localRay;
    localRay.origin = localOrigin;
    localRay.direction = localDir;
    
    // Test against element's bounding box
    return intersectAABB(localRay, element->boundsMin, element->boundsMax, t);
}

bool Raycast::intersectPlane(const Ray& ray, const glm::vec3& planePos, const glm::vec3& planeNormal,
                            float& t) {
    float denom = glm::dot(planeNormal, ray.direction);
    if (std::abs(denom) < 1e-6f) return false;
    t = glm::dot(planePos - ray.origin, planeNormal) / denom;
    return t >= 0.0f;
}

float Raycast::rayToSegmentSqDist(const Ray& ray, const glm::vec3& segA, const glm::vec3& segB,
                                  float& outTRay, float& outTSeg) {
    glm::vec3 segDir = segB - segA;
    float segLen = glm::length(segDir);
    if (segLen < 1e-6f) {
        glm::vec3 toA = segA - ray.origin;
        outTRay = glm::dot(toA, ray.direction);
        outTSeg = 0.0f;
        glm::vec3 d = toA - ray.direction * outTRay;
        return glm::dot(d, d);
    }
    glm::vec3 segUnit = segDir / segLen;
    glm::vec3 w0 = ray.origin - segA;
    float a = glm::dot(ray.direction, ray.direction);
    float b = glm::dot(ray.direction, segUnit);
    float c = glm::dot(segUnit, segUnit);
    float d = glm::dot(ray.direction, w0);
    float e = glm::dot(segUnit, w0);
    float denom = a * c - b * b;
    float tRay, tSeg;
    if (denom < 1e-6f) {
        tSeg = 0.0f;
        tRay = (b > 0 ? -d / a : 0.0f);
        tRay = std::max(0.0f, tRay);
    } else {
        tRay = (b * e - c * d) / denom;
        tSeg = (a * e - b * d) / denom;
        tSeg = std::max(0.0f, std::min(segLen, tSeg));
        if (tRay < 0.0f) {
            tRay = 0.0f;
            tSeg = std::max(0.0f, std::min(segLen, glm::dot(segUnit, ray.origin - segA)));
        }
    }
    glm::vec3 pRay = ray.origin + tRay * ray.direction;
    glm::vec3 pSeg = segA + tSeg * segUnit;
    outTRay = tRay;
    outTSeg = tSeg;
    glm::vec3 diff = pRay - pSeg;
    return glm::dot(diff, diff);
}

} // namespace opticsketch
