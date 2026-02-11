#include "render/beam.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace opticsketch {

Beam::Beam(const std::string& beamId) : id(beamId.empty() ? generateId() : beamId) {
    label = "Beam " + id;
}

glm::vec3 Beam::getDirection() const {
    glm::vec3 dir = end - start;
    float len = glm::length(dir);
    if (len > 1e-6f) {
        return dir / len;
    }
    return glm::vec3(1.0f, 0.0f, 0.0f); // Default direction
}

float Beam::getLength() const {
    return glm::length(end - start);
}

std::string Beam::generateId() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "beam_" << std::setfill('0') << std::setw(4) << counter++;
    return oss.str();
}

} // namespace opticsketch
