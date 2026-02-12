#pragma once

#include <imgui.h>
#include <string>

struct GLFWwindow;

namespace opticsketch {

class Scene;
class UndoStack;

class TemplatePanel {
public:
    void render(Scene* scene, UndoStack* undoStack, std::string* projectPath, GLFWwindow* window);
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }
private:
    bool visible = false;
    std::string selectedCategory = "All";
    bool confirmOpen = false;
    std::string pendingTemplateId;
};

} // namespace opticsketch
