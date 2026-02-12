#include "elements/annotation.h"
#include <atomic>

namespace opticsketch {

static std::atomic<int> s_annotationCounter{1};

Annotation::Annotation() : id(generateId()), label("Annotation"), text("Text") {}

Annotation::Annotation(const std::string& annotationId)
    : id(annotationId), label("Annotation"), text("Text") {}

std::unique_ptr<Annotation> Annotation::clone() const {
    auto a = std::make_unique<Annotation>(generateId());
    a->label = label;
    a->text = text;
    a->position = position;
    a->color = color;
    a->fontSize = fontSize;
    a->visible = visible;
    a->layer = layer;
    return a;
}

std::string Annotation::generateId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "ann_%04d", s_annotationCounter.fetch_add(1));
    return std::string(buf);
}

} // namespace opticsketch
