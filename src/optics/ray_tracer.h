#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <string>

namespace opticsketch {

class Scene;
class Element;
class Beam;

struct TraceConfig {
    int maxBounces = 20;
    float maxDistance = 5000.0f;   // mm
    float minIntensity = 0.01f;   // stop tracing when intensity drops below this
    float epsilon = 0.01f;        // offset to avoid self-intersection
};

struct TraceSegment {
    glm::vec3 start;
    glm::vec3 end;
    glm::vec3 color;
    float intensity;
    std::string sourceElementId;
};

class RayTracer {
public:
    // Trace all Source elements in the scene, creating beam segments
    void traceScene(Scene* scene, const TraceConfig& config = TraceConfig());

private:
    struct TraceRay {
        glm::vec3 origin;
        glm::vec3 direction;
        float intensity;
        glm::vec3 color;
        std::string sourceId;
        float wavelength = 633e-9f;  // meters (default HeNe red)
    };

    void traceRay(const TraceRay& ray, Scene* scene, const TraceConfig& config,
                  int depth, std::vector<TraceSegment>& segments);

    // Snell's law refraction. Returns false if total internal reflection occurs.
    static bool refract(const glm::vec3& incident, const glm::vec3& normal,
                        float n1, float n2, glm::vec3& refracted);

    // Reflection
    static glm::vec3 reflect(const glm::vec3& incident, const glm::vec3& normal);

    // Fresnel reflectance (Schlick approximation)
    static float fresnelSchlick(float cosTheta, float n1, float n2);
};

} // namespace opticsketch
