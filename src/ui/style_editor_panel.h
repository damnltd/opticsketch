#pragma once

#include <imgui.h>
#include <string>

namespace opticsketch {

struct SceneStyle;

class StyleEditorPanel {
public:
    void render(SceneStyle* style);
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    // Returns non-empty path if user just loaded a style preset (caller may need to refresh)
    const std::string& getLastLoadedPath() const { return lastLoadedPath; }
    void clearLastLoadedPath() { lastLoadedPath.clear(); }
private:
    bool visible = false;
    std::string lastLoadedPath;
};

} // namespace opticsketch
