#pragma once

#include <imgui.h>

namespace opticsketch {

class Scene;

// Maya/Blender-style outliner: flat list of scene objects. Click to select.
class OutlinerPanel {
public:
    OutlinerPanel() = default;

    void render(Scene* scene);

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

private:
    bool visible = true;
};

} // namespace opticsketch
