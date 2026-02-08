#pragma once

#include <imgui.h>

namespace opticsketch {

class Scene;

class PropertiesPanel {
public:
    PropertiesPanel() = default;

    void render(Scene* scene);

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

private:
    bool visible = true;
};

} // namespace opticsketch
