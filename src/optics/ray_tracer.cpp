#include "optics/ray_tracer.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "render/beam.h"
#include "render/raycast.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cmath>
#include <algorithm>

namespace opticsketch {

glm::vec3 RayTracer::reflect(const glm::vec3& incident, const glm::vec3& normal) {
    return incident - 2.0f * glm::dot(incident, normal) * normal;
}

bool RayTracer::refract(const glm::vec3& incident, const glm::vec3& normal,
                        float n1, float n2, glm::vec3& refracted) {
    float ratio = n1 / n2;
    float cosI = -glm::dot(incident, normal);
    float sin2T = ratio * ratio * (1.0f - cosI * cosI);

    if (sin2T > 1.0f) {
        // Total internal reflection
        return false;
    }

    float cosT = std::sqrt(1.0f - sin2T);
    refracted = ratio * incident + (ratio * cosI - cosT) * normal;
    refracted = glm::normalize(refracted);
    return true;
}

float RayTracer::fresnelSchlick(float cosTheta, float n1, float n2) {
    float r0 = (n1 - n2) / (n1 + n2);
    r0 = r0 * r0;
    float x = 1.0f - cosTheta;
    return r0 + (1.0f - r0) * x * x * x * x * x;
}

void RayTracer::traceScene(Scene* scene, const TraceConfig& config) {
    if (!scene) return;

    // Clear previous traced beams
    scene->clearTracedBeams();

    std::vector<TraceSegment> segments;

    // Find all Source elements and fire rays from them
    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;
        if (elem->optics.opticalType != OpticalType::Source) continue;

        // Fire ray along element's local +Z axis (forward direction)
        glm::mat4 model = elem->transform.getMatrix();
        glm::vec3 forward = glm::normalize(glm::vec3(model * glm::vec4(0, 0, 1, 0)));
        glm::vec3 origin = elem->getWorldBoundsCenter();

        // Offset origin slightly along forward to avoid self-intersection
        origin += forward * config.epsilon;

        TraceRay ray;
        ray.origin = origin;
        ray.direction = forward;
        ray.intensity = 1.0f;
        ray.color = glm::vec3(1.0f, 0.0f, 0.0f); // Default laser red
        ray.sourceId = elem->id;

        traceRay(ray, scene, config, 0, segments);
    }

    // Convert trace segments into Beam objects
    for (const auto& seg : segments) {
        auto beam = std::make_unique<Beam>();
        beam->start = seg.start;
        beam->end = seg.end;
        beam->color = seg.color;
        beam->width = 2.0f;
        beam->isTraced = true;
        beam->sourceElementId = seg.sourceElementId;
        scene->addBeam(std::move(beam));
    }
}

void RayTracer::traceRay(const TraceRay& ray, Scene* scene, const TraceConfig& config,
                          int depth, std::vector<TraceSegment>& segments) {
    if (depth >= config.maxBounces) return;
    if (ray.intensity < config.minIntensity) return;

    // Find closest element intersection
    float closestT = config.maxDistance;
    Element* hitElement = nullptr;
    glm::vec3 hitNormalWorld(0.0f);

    for (const auto& elem : scene->getElements()) {
        if (!elem->visible) continue;
        // Skip the source element itself on the first bounce to avoid self-hit
        if (elem->optics.opticalType == OpticalType::Source && depth == 0 && elem->id == ray.sourceId) continue;
        // Skip passive/source elements for intersection (they don't interact with rays)
        // Actually, passive elements should just pass through, but we still need to detect them
        // to know to keep going. Let's include all visible elements.

        glm::mat4 model = elem->transform.getMatrix();
        glm::mat4 invModel = glm::inverse(model);

        // Transform ray to local space
        glm::vec3 localOrigin = glm::vec3(invModel * glm::vec4(ray.origin, 1.0f));
        glm::vec3 localDir = glm::normalize(glm::vec3(invModel * glm::vec4(ray.direction, 0.0f)));

        Raycast::Ray localRay{localOrigin, localDir};
        float t;
        glm::vec3 localNormal;

        if (Raycast::intersectAABBWithNormal(localRay, elem->boundsMin, elem->boundsMax, t, localNormal)) {
            if (t > config.epsilon && t < closestT) {
                closestT = t;
                hitElement = elem.get();
                // Transform normal to world space
                glm::mat3 normalMatrix = glm::mat3(glm::transpose(invModel));
                hitNormalWorld = glm::normalize(normalMatrix * localNormal);
            }
        }
    }

    // Create a beam segment from ray origin to hit point (or max distance)
    glm::vec3 endPoint = ray.origin + ray.direction * closestT;

    TraceSegment seg;
    seg.start = ray.origin;
    seg.end = endPoint;
    seg.color = ray.color;
    seg.intensity = ray.intensity;
    seg.sourceElementId = ray.sourceId;
    segments.push_back(seg);

    if (!hitElement) return; // Ray escaped the scene

    // Ensure normal faces against the ray direction
    if (glm::dot(hitNormalWorld, ray.direction) > 0.0f) {
        hitNormalWorld = -hitNormalWorld;
    }

    glm::vec3 hitPoint = endPoint;
    const OpticalProperties& optics = hitElement->optics;

    switch (optics.opticalType) {
        case OpticalType::Mirror: {
            // Pure reflection
            glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
            TraceRay newRay;
            newRay.origin = hitPoint + reflected * config.epsilon;
            newRay.direction = reflected;
            newRay.intensity = ray.intensity * optics.reflectivity;
            newRay.color = ray.color;
            newRay.sourceId = ray.sourceId;
            traceRay(newRay, scene, config, depth + 1, segments);
            break;
        }

        case OpticalType::Lens: {
            // Thin lens approximation: refract at the surface using Snell's law
            // For a thin lens, we deflect the ray toward the focal point
            float n1 = 1.0f;  // air
            float n2 = optics.ior;

            glm::vec3 refracted;
            if (refract(ray.direction, hitNormalWorld, n1, n2, refracted)) {
                // Compute Fresnel reflectance
                float cosI = std::abs(glm::dot(ray.direction, hitNormalWorld));
                float R = fresnelSchlick(cosI, n1, n2);

                // Reflected component
                if (R * ray.intensity > config.minIntensity) {
                    glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
                    TraceRay reflRay;
                    reflRay.origin = hitPoint + reflected * config.epsilon;
                    reflRay.direction = reflected;
                    reflRay.intensity = ray.intensity * R;
                    reflRay.color = ray.color;
                    reflRay.sourceId = ray.sourceId;
                    traceRay(reflRay, scene, config, depth + 1, segments);
                }

                // Transmitted component
                TraceRay transRay;
                transRay.origin = hitPoint + refracted * config.epsilon;
                transRay.direction = refracted;
                transRay.intensity = ray.intensity * (1.0f - R) * optics.transmissivity;
                transRay.color = ray.color;
                transRay.sourceId = ray.sourceId;
                traceRay(transRay, scene, config, depth + 1, segments);
            } else {
                // Total internal reflection
                glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
                TraceRay newRay;
                newRay.origin = hitPoint + reflected * config.epsilon;
                newRay.direction = reflected;
                newRay.intensity = ray.intensity;
                newRay.color = ray.color;
                newRay.sourceId = ray.sourceId;
                traceRay(newRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Splitter: {
            // Both reflect and transmit
            float R = optics.reflectivity;
            float T = optics.transmissivity;

            // Reflected
            if (R * ray.intensity > config.minIntensity) {
                glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
                TraceRay reflRay;
                reflRay.origin = hitPoint + reflected * config.epsilon;
                reflRay.direction = reflected;
                reflRay.intensity = ray.intensity * R;
                reflRay.color = ray.color;
                reflRay.sourceId = ray.sourceId;
                traceRay(reflRay, scene, config, depth + 1, segments);
            }

            // Transmitted (continues in same direction through thin splitter)
            if (T * ray.intensity > config.minIntensity) {
                TraceRay transRay;
                transRay.origin = hitPoint + ray.direction * config.epsilon;
                transRay.direction = ray.direction;
                transRay.intensity = ray.intensity * T;
                transRay.color = ray.color;
                transRay.sourceId = ray.sourceId;
                traceRay(transRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Prism: {
            // Refract through prism surface
            float n1 = 1.0f;
            float n2 = optics.ior;

            glm::vec3 refracted;
            if (refract(ray.direction, hitNormalWorld, n1, n2, refracted)) {
                TraceRay transRay;
                transRay.origin = hitPoint + refracted * config.epsilon;
                transRay.direction = refracted;
                transRay.intensity = ray.intensity * optics.transmissivity;
                transRay.color = ray.color;
                transRay.sourceId = ray.sourceId;
                traceRay(transRay, scene, config, depth + 1, segments);
            } else {
                // Total internal reflection at prism surface
                glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
                TraceRay newRay;
                newRay.origin = hitPoint + reflected * config.epsilon;
                newRay.direction = reflected;
                newRay.intensity = ray.intensity;
                newRay.color = ray.color;
                newRay.sourceId = ray.sourceId;
                traceRay(newRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Absorber: {
            // Ray terminates here
            break;
        }

        case OpticalType::Grating: {
            // Simplified: treat as partial reflector
            float R = 0.3f;
            glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
            if (R * ray.intensity > config.minIntensity) {
                TraceRay reflRay;
                reflRay.origin = hitPoint + reflected * config.epsilon;
                reflRay.direction = reflected;
                reflRay.intensity = ray.intensity * R;
                reflRay.color = ray.color;
                reflRay.sourceId = ray.sourceId;
                traceRay(reflRay, scene, config, depth + 1, segments);
            }
            // Transmitted 0th order
            if ((1.0f - R) * ray.intensity > config.minIntensity) {
                TraceRay transRay;
                transRay.origin = hitPoint + ray.direction * config.epsilon;
                transRay.direction = ray.direction;
                transRay.intensity = ray.intensity * (1.0f - R);
                transRay.color = ray.color;
                transRay.sourceId = ray.sourceId;
                traceRay(transRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Source:
        case OpticalType::Passive:
        default: {
            // Pass through
            TraceRay passRay;
            passRay.origin = hitPoint + ray.direction * config.epsilon;
            passRay.direction = ray.direction;
            passRay.intensity = ray.intensity;
            passRay.color = ray.color;
            passRay.sourceId = ray.sourceId;
            traceRay(passRay, scene, config, depth + 1, segments);
            break;
        }
    }
}

} // namespace opticsketch
