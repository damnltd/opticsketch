#pragma once

#include <string>

namespace opticsketch {

class Scene;
struct SceneStyle;

struct SvgExportOptions {
    bool showOpticalAxis = true;
    bool showScaleBar = true;
};

// Export scene as an SVG file.
// Projects the XZ plane (top-down view) into 2D SVG coordinates.
// Returns true on success.
bool exportSvg(const std::string& path, Scene* scene, SceneStyle* style,
               const SvgExportOptions& opts = {});

} // namespace opticsketch
