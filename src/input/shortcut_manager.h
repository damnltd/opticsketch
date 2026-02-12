#pragma once

#include <string>
#include <vector>
#include <unordered_map>

struct GLFWwindow;

namespace opticsketch {

struct KeyBinding {
    std::string actionId;
    std::string displayName;
    std::string category;
    int key;            // GLFW key code
    bool ctrl = false;
    bool shift = false;
    bool alt = false;
};

class ShortcutManager {
public:
    void registerDefaults();
    void updateKeyStates(GLFWwindow* window);

    // Returns true on the frame a binding's key combo transitions from up to down
    bool justPressed(const std::string& actionId) const;

    // Human-readable string for a binding (e.g. "Ctrl+S")
    std::string getDisplayString(const std::string& actionId) const;

    void rebind(const std::string& actionId, int key, bool ctrl, bool shift, bool alt);
    const std::vector<KeyBinding>& getBindings() const { return bindings; }

    void saveToFile(const std::string& path) const;
    void loadFromFile(const std::string& path);

    static std::string keyToString(int glfwKey);

private:
    std::vector<KeyBinding> bindings;
    std::unordered_map<std::string, size_t> actionIndex; // actionId -> index in bindings

    // Edge detection
    std::unordered_map<int, bool> prevKeyState;
    std::unordered_map<int, bool> curKeyState;
    bool curCtrl = false, curShift = false, curAlt = false;

    void rebuildIndex();
};

} // namespace opticsketch
