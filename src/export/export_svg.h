#pragma once

#include <string>

namespace opticsketch {

class Scene;
struct SceneStyle;

// Export scene as an SVG file.
// Projects the XZ plane (top-down view) into 2D SVG coordinates.
// Returns true on success.
bool exportSvg(const std::string& path, Scene* scene, SceneStyle* style);

} // namespace opticsketch
