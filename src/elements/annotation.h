#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace opticsketch {

class Annotation {
public:
    Annotation();
    explicit Annotation(const std::string& annotationId);

    std::string id;
    std::string label;
    std::string text;
    glm::vec3 position{0.0f};
    glm::vec3 color{0.95f, 0.95f, 0.85f};
    float fontSize = 14.0f;
    bool visible = true;
    int layer = 0;

    std::unique_ptr<Annotation> clone() const;
    static std::string generateId();
};

} // namespace opticsketch
