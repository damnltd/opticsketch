#pragma once

#include <imgui.h>

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
    
private:
    ToolMode currentTool = ToolMode::Select;
    bool visible = true;
    
    void renderToolButtons();
};

} // namespace opticsketch
