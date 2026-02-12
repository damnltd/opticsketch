#pragma once

#include <imgui.h>

namespace opticsketch {

struct SceneStyle;

class StyleEditorPanel {
public:
    void render(SceneStyle* style);
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }
private:
    bool visible = false;
};

} // namespace opticsketch
