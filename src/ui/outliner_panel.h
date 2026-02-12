#pragma once

#include <imgui.h>

namespace opticsketch {

class Scene;

// Maya/Blender-style outliner: flat list of scene objects with multi-select.
// Click = single select, Ctrl+Click = toggle, Shift+Click = range select.
class OutlinerPanel {
public:
    OutlinerPanel() = default;

    void render(Scene* scene);

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

private:
    bool visible = true;
    int lastClickedIndex = -1;  // flat index for Shift+click range selection
};

} // namespace opticsketch
