#pragma once

#include <string>

namespace opticsketch {

class Scene;
struct SceneStyle;

// Export scene as a standalone TikZ/LaTeX document (.tex).
// Projects the XZ plane (top-down view) into 2D tikz coordinates.
// Returns true on success.
bool exportTikz(const std::string& path, Scene* scene, SceneStyle* style);

} // namespace opticsketch
