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

std::unique_ptr<Beam> Beam::clone() const {
    auto b = std::make_unique<Beam>(); // auto-generates new ID
    b->label = label;
    b->start = start;
    b->end = end;
    b->color = color;
    b->width = width;
    b->visible = visible;
    b->layer = layer;
    b->isTraced = isTraced;
    b->sourceElementId = sourceElementId;
    b->isGaussian = isGaussian;
    b->waistW0 = waistW0;
    b->wavelength = wavelength;
    b->waistPosition = waistPosition;
    return b;
}

float Beam::getRayleighRange() const {
    if (wavelength < 1e-15f) return 0.0f;
    return 3.14159265f * waistW0 * waistW0 / wavelength;
}

float Beam::getDivergenceAngle() const {
    if (waistW0 < 1e-15f) return 0.0f;
    return wavelength / (3.14159265f * waistW0);
}

float Beam::beamRadiusAt(float z) const {
    float zR = getRayleighRange();
    if (zR < 1e-15f) return waistW0;
    return waistW0 * std::sqrt(1.0f + (z / zR) * (z / zR));
}

std::string Beam::generateId() {
    static int counter = 0;
    std::ostringstream oss;
    oss << "beam_" << std::setfill('0') << std::setw(4) << counter++;
    return oss.str();
}

} // namespace opticsketch
