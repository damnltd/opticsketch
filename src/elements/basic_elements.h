#pragma once

#include "elements/element.h"
#include <memory>

namespace opticsketch {

// Basic element factory functions
std::unique_ptr<Element> createLaser(const std::string& id = "");
std::unique_ptr<Element> createMirror(const std::string& id = "");
std::unique_ptr<Element> createLens(const std::string& id = "");
std::unique_ptr<Element> createBeamSplitter(const std::string& id = "");
std::unique_ptr<Element> createDetector(const std::string& id = "");

// Helper to create element from type
std::unique_ptr<Element> createElement(ElementType type, const std::string& id = "");

} // namespace opticsketch
