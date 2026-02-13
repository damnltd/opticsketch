#pragma once

#include <imgui.h>
#include "render/gizmo.h"

namespace opticsketch {

enum class ToolMode {
    Select,
    Move,
    Rotate,
    Scale,
    DrawBeam,
    PlaceAnnotation,
    Measure
};

class ToolboxPanel {
public:
    ToolboxPanel();

    // Render the toolbox panel UI
    void render();

    // Get current tool mode
    ToolMode getCurrentTool() const { return currentTool; }

    // Check if toolbox is visible
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }

    // Set current tool
    void setTool(ToolMode tool) { currentTool = tool; }

    // Gizmo space (World / Local)
    GizmoSpace getGizmoSpace() const { return gizmoSpace; }
    void setGizmoSpace(GizmoSpace s) { gizmoSpace = s; }
    void toggleGizmoSpace() { gizmoSpace = (gizmoSpace == GizmoSpace::World) ? GizmoSpace::Local : GizmoSpace::World; }

private:
    ToolMode currentTool = ToolMode::Select;
    GizmoSpace gizmoSpace = GizmoSpace::World;
    bool visible = true;

    void renderToolButtons();
};

} // namespace opticsketch
