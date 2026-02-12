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
std::unique_ptr<Element> createFilter(const std::string& id = "");
std::unique_ptr<Element> createAperture(const std::string& id = "");
std::unique_ptr<Element> createPrism(const std::string& id = "");
std::unique_ptr<Element> createPrismRA(const std::string& id = "");
std::unique_ptr<Element> createGrating(const std::string& id = "");
std::unique_ptr<Element> createFiberCoupler(const std::string& id = "");
std::unique_ptr<Element> createScreen(const std::string& id = "");
std::unique_ptr<Element> createMount(const std::string& id = "");

// Create mesh element from OBJ file path
std::unique_ptr<Element> createMeshElement(const std::string& objPath, const std::string& id = "");

// Helper to create element from type
std::unique_ptr<Element> createElement(ElementType type, const std::string& id = "");

} // namespace opticsketch
