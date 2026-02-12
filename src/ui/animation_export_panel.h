#pragma once

#include <imgui.h>
#include "export/export_animation.h"

namespace opticsketch {

class Scene;
class Viewport;
struct SceneStyle;

class AnimationExportPanel {
public:
    void render(Viewport* viewport, Scene* scene, SceneStyle* style);

    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }
    void show() { visible = true; }

    bool isExporting() const { return exportState.active; }
    const AnimationExportState& getExportState() const { return exportState; }

    // Call each frame when exporting — advances one animation frame
    bool advanceExport(Viewport* viewport, Scene* scene, SceneStyle* style);

private:
    bool visible = false;
    AnimationExportSettings settings;
    AnimationExportState exportState;
    bool ffmpegAvailable = false;
    bool checkedFFmpeg = false;
};

} // namespace opticsketch
