#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace opticsketch {

class Measurement {
public:
    Measurement();
    explicit Measurement(const std::string& measId);

    std::string id;
    std::string label;
    glm::vec3 startPoint{0.0f};
    glm::vec3 endPoint{0.0f};
    glm::vec3 color{0.9f, 0.9f, 0.3f};
    float fontSize = 12.0f;
    bool visible = true;
    int layer = 0;

    float getDistance() const;
    std::unique_ptr<Measurement> clone() const;
    static std::string generateId();
};

} // namespace opticsketch
