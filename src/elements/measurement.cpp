#include "elements/measurement.h"
#include <atomic>
#include <cstdio>

namespace opticsketch {

static std::atomic<int> s_measurementCounter{1};

Measurement::Measurement() : id(generateId()), label("Measurement") {}

Measurement::Measurement(const std::string& measId)
    : id(measId), label("Measurement") {}

float Measurement::getDistance() const {
    return glm::length(endPoint - startPoint);
}

std::unique_ptr<Measurement> Measurement::clone() const {
    auto m = std::make_unique<Measurement>(generateId());
    m->label = label;
    m->startPoint = startPoint;
    m->endPoint = endPoint;
    m->color = color;
    m->fontSize = fontSize;
    m->visible = visible;
    m->layer = layer;
    return m;
}

std::string Measurement::generateId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "meas_%04d", s_measurementCounter.fetch_add(1));
    return std::string(buf);
}

} // namespace opticsketch
