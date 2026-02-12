#include "optics/ray_tracer.h"
#include "scene/scene.h"
#include "elements/element.h"
#include "render/beam.h"
#include "render/raycast.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/quaternion.hpp>
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

// Convert wavelength in meters to visible spectrum RGB color
// Based on Dan Bruton's approximate CIE algorithm
static glm::vec3 wavelengthToRGB(float wavelength) {
    float nm = wavelength * 1e9f; // convert to nanometers
    float r = 0.0f, g = 0.0f, b = 0.0f;

    if (nm >= 380.0f && nm < 440.0f) {
        r = -(nm - 440.0f) / (440.0f - 380.0f);
        g = 0.0f;
        b = 1.0f;
    } else if (nm >= 440.0f && nm < 490.0f) {
        r = 0.0f;
        g = (nm - 440.0f) / (490.0f - 440.0f);
        b = 1.0f;
    } else if (nm >= 490.0f && nm < 510.0f) {
        r = 0.0f;
        g = 1.0f;
        b = -(nm - 510.0f) / (510.0f - 490.0f);
    } else if (nm >= 510.0f && nm < 580.0f) {
        r = (nm - 510.0f) / (580.0f - 510.0f);
        g = 1.0f;
        b = 0.0f;
    } else if (nm >= 580.0f && nm < 645.0f) {
        r = 1.0f;
        g = -(nm - 645.0f) / (645.0f - 580.0f);
        b = 0.0f;
    } else if (nm >= 645.0f && nm <= 780.0f) {
        r = 1.0f;
        g = 0.0f;
        b = 0.0f;
    }

    // Intensity falloff at edges of visible spectrum
    float factor = 1.0f;
    if (nm >= 380.0f && nm < 420.0f) {
        factor = 0.3f + 0.7f * (nm - 380.0f) / (420.0f - 380.0f);
    } else if (nm >= 700.0f && nm <= 780.0f) {
        factor = 0.3f + 0.7f * (780.0f - nm) / (780.0f - 700.0f);
    }

    return glm::vec3(r * factor, g * factor, b * factor);
}

// Cauchy dispersion: n(lambda) = baseIOR + B / lambda^2
// baseIOR is the IOR at a reference wavelength (e.g. 633nm)
// cauchyB is in m^2 units. Set to 0 for no dispersion.
static float dispersionIOR(float baseIOR, float cauchyB, float wavelength) {
    if (cauchyB == 0.0f || wavelength < 1e-12f) return baseIOR;
    // Compute offset from reference wavelength (633nm)
    float lambdaRef = 633e-9f;
    float nRef = baseIOR; // baseIOR is defined at the reference wavelength
    // n(lambda) = A + B/lambda^2, where A = nRef - B/lambdaRef^2
    return nRef + cauchyB * (1.0f / (wavelength * wavelength) - 1.0f / (lambdaRef * lambdaRef));
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

        float baseWavelength = 633e-9f; // Default HeNe red
        int rayCount = std::max(1, elem->optics.sourceRayCount);
        float beamWidth = elem->optics.sourceBeamWidth;

        // Determine wavelengths to trace
        struct WavelengthEntry { float lambda; float intensityScale; };
        std::vector<WavelengthEntry> wavelengths;

        if (elem->optics.sourceIsWhiteLight) {
            // 7 spectral wavelengths spanning visible range
            const float spectra[] = { 380e-9f, 450e-9f, 490e-9f, 530e-9f, 580e-9f, 620e-9f, 700e-9f };
            for (float lam : spectra) {
                wavelengths.push_back({lam, 1.0f / 7.0f});
            }
        } else {
            wavelengths.push_back({baseWavelength, 1.0f});
        }

        // For each wavelength, fire rayCount parallel rays across beam width
        for (const auto& wl : wavelengths) {
            for (int ri = 0; ri < rayCount; ri++) {
                // Compute lateral offset for multi-ray mode
                glm::vec3 offset(0.0f);
                if (rayCount > 1 && beamWidth > 0.0f) {
                    // Spread rays along local Y axis (perpendicular to forward)
                    glm::vec3 localUp = glm::normalize(glm::vec3(model * glm::vec4(0, 1, 0, 0)));
                    float t = static_cast<float>(ri) / static_cast<float>(rayCount - 1) - 0.5f; // -0.5 to +0.5
                    offset = localUp * (t * beamWidth);
                }

                TraceRay ray;
                ray.origin = origin + offset;
                ray.direction = forward;
                ray.intensity = wl.intensityScale;
                ray.wavelength = wl.lambda;
                ray.color = wavelengthToRGB(wl.lambda);
                ray.sourceId = elem->id;

                traceRay(ray, scene, config, 0, segments);
            }
        }
    }

    // Convert trace segments into Beam objects
    for (const auto& seg : segments) {
        auto beam = std::make_unique<Beam>();
        beam->start = seg.start;
        beam->end = seg.end;
        beam->color = seg.color;
        beam->width = 2.0f;
        beam->isTraced = true;
        beam->intensity = seg.intensity;
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
            newRay.wavelength = ray.wavelength;
            traceRay(newRay, scene, config, depth + 1, segments);
            break;
        }

        case OpticalType::Lens: {
            // Thin lens model using focalLength with wavelength-dependent dispersion
            glm::mat4 elemModel = hitElement->transform.getMatrix();
            glm::vec3 opticalAxis = glm::normalize(glm::vec3(elemModel * glm::vec4(0, 0, 1, 0)));
            glm::vec3 elemCenter = hitElement->getWorldBoundsCenter();

            // Project hit point onto the lens plane to get displacement from optical axis
            glm::vec3 toHit = hitPoint - elemCenter;
            glm::vec3 offset = toHit - glm::dot(toHit, opticalAxis) * opticalAxis;
            float h = glm::length(offset);

            // Wavelength-dependent IOR and focal length
            float n = dispersionIOR(optics.ior, optics.cauchyB, ray.wavelength);
            // Focal length scales inversely with (n-1): f(λ) = f_ref * (n_ref - 1) / (n(λ) - 1)
            float nRef = optics.ior; // IOR at reference wavelength
            float f = optics.focalLength;
            if (std::abs(n - 1.0f) > 1e-6f && std::abs(nRef - 1.0f) > 1e-6f) {
                f = optics.focalLength * (nRef - 1.0f) / (n - 1.0f);
            }

            // Compute Fresnel reflectance for reflected component
            float cosI = std::abs(glm::dot(ray.direction, hitNormalWorld));
            float R = fresnelSchlick(cosI, 1.0f, n);

            // Reflected component
            if (R * ray.intensity > config.minIntensity) {
                glm::vec3 reflected = reflect(ray.direction, hitNormalWorld);
                TraceRay reflRay;
                reflRay.origin = hitPoint + reflected * config.epsilon;
                reflRay.direction = reflected;
                reflRay.intensity = ray.intensity * R;
                reflRay.color = ray.color;
                reflRay.sourceId = ray.sourceId;
                reflRay.wavelength = ray.wavelength;
                traceRay(reflRay, scene, config, depth + 1, segments);
            }

            // Transmitted component with thin-lens deflection
            float T = (1.0f - R) * optics.transmissivity;
            if (T * ray.intensity > config.minIntensity) {
                glm::vec3 exitDir;
                if (std::abs(f) > 0.01f && h > 1e-6f) {
                    // Focal point on the exit side of the lens
                    // Determine which side the ray is coming from
                    float rayDotAxis = glm::dot(ray.direction, opticalAxis);
                    float sign = (rayDotAxis >= 0.0f) ? 1.0f : -1.0f;
                    glm::vec3 focalPoint = elemCenter + sign * opticalAxis * f;

                    // The exit ray goes from hitPoint toward focalPoint (for parallel rays)
                    // For general rays: use the thin lens equation
                    // exitDir = normalize(focalPoint - hitPoint) approximately
                    // More accurate: deflect by angle theta = -h/f
                    glm::vec3 toFocal = focalPoint - hitPoint;
                    exitDir = glm::normalize(toFocal);

                    // Blend with incident direction for rays not parallel to axis
                    // A ray through the center should pass undeviated
                    float blend = h / (glm::length(hitElement->boundsMax - hitElement->boundsMin) * 0.5f);
                    blend = std::clamp(blend, 0.0f, 1.0f);
                    exitDir = glm::normalize(glm::mix(ray.direction, exitDir, blend));
                } else {
                    // Ray through center or infinite focal length — pass straight through
                    exitDir = ray.direction;
                }

                TraceRay transRay;
                transRay.origin = hitPoint + exitDir * config.epsilon;
                transRay.direction = exitDir;
                transRay.intensity = ray.intensity * T;
                transRay.color = ray.color;
                transRay.sourceId = ray.sourceId;
                transRay.wavelength = ray.wavelength;
                traceRay(transRay, scene, config, depth + 1, segments);
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
                reflRay.wavelength = ray.wavelength;
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
                transRay.wavelength = ray.wavelength;
                traceRay(transRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Prism: {
            // Refract through prism surface with wavelength-dependent dispersion
            float n1 = 1.0f;
            float n2 = dispersionIOR(optics.ior, optics.cauchyB, ray.wavelength);

            glm::vec3 refracted;
            if (refract(ray.direction, hitNormalWorld, n1, n2, refracted)) {
                TraceRay transRay;
                transRay.origin = hitPoint + refracted * config.epsilon;
                transRay.direction = refracted;
                transRay.intensity = ray.intensity * optics.transmissivity;
                transRay.color = ray.color;
                transRay.sourceId = ray.sourceId;
                transRay.wavelength = ray.wavelength;
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
                newRay.wavelength = ray.wavelength;
                traceRay(newRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Absorber: {
            // Ray terminates here
            break;
        }

        case OpticalType::Grating: {
            // Diffraction grating using grating equation:
            // sin(theta_m) = sin(theta_i) + m * lambda * lineDensity * 1e3
            // (lineDensity in lines/mm, lambda in meters, factor 1e3 converts mm->m)
            float lineDensity = optics.gratingLineDensity; // lines/mm
            float lambda = ray.wavelength;                 // meters
            float d = 1.0f / (lineDensity * 1e3f);        // grating spacing in meters

            // Compute incident angle relative to grating normal
            float cosI = std::abs(glm::dot(ray.direction, hitNormalWorld));
            float sinI = std::sqrt(std::max(0.0f, 1.0f - cosI * cosI));
            // Sign of incidence
            if (glm::dot(ray.direction, hitNormalWorld) > 0.0f) sinI = -sinI;

            // Grating tangent direction (in the plane of incidence)
            glm::vec3 tangent = glm::normalize(ray.direction - glm::dot(ray.direction, hitNormalWorld) * hitNormalWorld);

            float intensityPerOrder = ray.intensity / 3.0f; // Split among 3 orders

            // Generate orders m = -1, 0, +1
            for (int m = -1; m <= 1; m++) {
                float sinM = sinI + static_cast<float>(m) * lambda / d;
                if (std::abs(sinM) > 1.0f) continue; // Evanescent order, skip

                if (intensityPerOrder < config.minIntensity) continue;

                glm::vec3 orderDir;
                if (m == 0) {
                    // 0th order: transmitted straight through
                    orderDir = ray.direction;
                } else {
                    float cosM = std::sqrt(std::max(0.0f, 1.0f - sinM * sinM));
                    // Reconstruct diffracted direction
                    // Normal component points away from surface (transmitted side)
                    orderDir = sinM * tangent - cosM * hitNormalWorld;
                    orderDir = glm::normalize(orderDir);
                }

                TraceRay orderRay;
                orderRay.origin = hitPoint + orderDir * config.epsilon;
                orderRay.direction = orderDir;
                orderRay.intensity = intensityPerOrder;
                orderRay.color = ray.color;
                orderRay.sourceId = ray.sourceId;
                orderRay.wavelength = ray.wavelength;
                traceRay(orderRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Filter: {
            // Transmit with attenuation and color tint
            float T = optics.transmissivity;
            if (T * ray.intensity > config.minIntensity) {
                TraceRay transRay;
                transRay.origin = hitPoint + ray.direction * config.epsilon;
                transRay.direction = ray.direction;
                transRay.intensity = ray.intensity * T;
                transRay.color = ray.color * optics.filterColor;
                transRay.sourceId = ray.sourceId;
                transRay.wavelength = ray.wavelength;
                traceRay(transRay, scene, config, depth + 1, segments);
            }
            break;
        }

        case OpticalType::Aperture: {
            // Check if hit point falls within the opening
            glm::mat4 elemModel = hitElement->transform.getMatrix();
            glm::mat4 invElemModel = glm::inverse(elemModel);
            glm::vec3 localHit = glm::vec3(invElemModel * glm::vec4(hitPoint, 1.0f));

            // Aperture opening is centered, extends apertureDiameter fraction of bounds height
            float boundsHeight = hitElement->boundsMax.y - hitElement->boundsMin.y;
            float boundsWidth = hitElement->boundsMax.x - hitElement->boundsMin.x;
            float openingHalfY = (optics.apertureDiameter * boundsHeight) * 0.5f;
            float openingHalfX = (optics.apertureDiameter * boundsWidth) * 0.5f;

            float localCenterY = (hitElement->boundsMin.y + hitElement->boundsMax.y) * 0.5f;
            float localCenterX = (hitElement->boundsMin.x + hitElement->boundsMax.x) * 0.5f;

            bool insideOpening = std::abs(localHit.y - localCenterY) < openingHalfY &&
                                 std::abs(localHit.x - localCenterX) < openingHalfX;

            if (insideOpening) {
                // Pass through the opening
                TraceRay passRay;
                passRay.origin = hitPoint + ray.direction * config.epsilon;
                passRay.direction = ray.direction;
                passRay.intensity = ray.intensity;
                passRay.color = ray.color;
                passRay.sourceId = ray.sourceId;
                passRay.wavelength = ray.wavelength;
                traceRay(passRay, scene, config, depth + 1, segments);
            }
            // else: ray is absorbed by the aperture body
            break;
        }

        case OpticalType::FiberCoupler: {
            // Absorb incoming ray, emit along element's local +Z axis
            glm::mat4 elemModel = hitElement->transform.getMatrix();
            glm::vec3 fiberAxis = glm::normalize(glm::vec3(elemModel * glm::vec4(0, 0, 1, 0)));
            glm::vec3 elemCenter = hitElement->getWorldBoundsCenter();

            float T = optics.transmissivity; // coupling efficiency
            if (T * ray.intensity > config.minIntensity) {
                TraceRay fiberRay;
                fiberRay.origin = elemCenter + fiberAxis * config.epsilon;
                fiberRay.direction = fiberAxis;
                fiberRay.intensity = ray.intensity * T;
                fiberRay.color = ray.color;
                fiberRay.sourceId = ray.sourceId;
                fiberRay.wavelength = ray.wavelength;
                traceRay(fiberRay, scene, config, depth + 1, segments);
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
            passRay.wavelength = ray.wavelength;
            traceRay(passRay, scene, config, depth + 1, segments);
            break;
        }
    }
}

} // namespace opticsketch
