#pragma once

#include <imgui.h>
#include <string>

namespace opticsketch {

class ShortcutManager;

class ShortcutsPanel {
public:
    void render(ShortcutManager* mgr);
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }
private:
    bool visible = false;
    bool rebinding = false;
    std::string rebindActionId;
};

} // namespace opticsketch
