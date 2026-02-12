#include "input/shortcut_manager.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace opticsketch {

void ShortcutManager::registerDefaults() {
    bindings.clear();
    // Tools
    bindings.push_back({"tool.select",      "Select Tool",      "Tools", GLFW_KEY_Q});
    bindings.push_back({"tool.move",         "Move Tool",        "Tools", GLFW_KEY_W});
    bindings.push_back({"tool.rotate",       "Rotate Tool",      "Tools", GLFW_KEY_E});
    bindings.push_back({"tool.scale",        "Scale Tool",       "Tools", GLFW_KEY_R});
    bindings.push_back({"tool.beam",         "Draw Beam",        "Tools", GLFW_KEY_B});
    bindings.push_back({"tool.annotation",   "Place Annotation", "Tools", GLFW_KEY_T});
    bindings.push_back({"tool.measure",      "Measure Tool",     "Tools", GLFW_KEY_M});

    // Snap
    bindings.push_back({"snap.toggle_grid",  "Toggle Grid Snap", "Snap",  GLFW_KEY_X});

    // File
    bindings.push_back({"file.new",       "New Project",     "File", GLFW_KEY_N, true});
    bindings.push_back({"file.open",      "Open Project",    "File", GLFW_KEY_O, true});
    bindings.push_back({"file.save",      "Save Project",    "File", GLFW_KEY_S, true});
    bindings.push_back({"file.save_as",   "Save As...",      "File", GLFW_KEY_S, true, true});
    bindings.push_back({"file.export_png","Export PNG",       "File", GLFW_KEY_E, true});

    // Edit
    bindings.push_back({"edit.undo",       "Undo",       "Edit", GLFW_KEY_Z, true});
    bindings.push_back({"edit.redo",       "Redo",       "Edit", GLFW_KEY_Y, true});
    bindings.push_back({"edit.copy",       "Copy",       "Edit", GLFW_KEY_C, true});
    bindings.push_back({"edit.cut",        "Cut",        "Edit", GLFW_KEY_X, true});
    bindings.push_back({"edit.paste",      "Paste",      "Edit", GLFW_KEY_V, true});
    bindings.push_back({"edit.delete",     "Delete",     "Edit", GLFW_KEY_DELETE});
    bindings.push_back({"edit.delete_alt", "Delete (alt)","Edit", GLFW_KEY_BACKSPACE});
    bindings.push_back({"edit.select_all", "Select All", "Edit", GLFW_KEY_A, true});
    bindings.push_back({"edit.group",      "Group",      "Edit", GLFW_KEY_G, true});
    bindings.push_back({"edit.ungroup",    "Ungroup",    "Edit", GLFW_KEY_G, true, true});

    // View
    bindings.push_back({"view.reset",          "Reset View",     "View", GLFW_KEY_HOME});
    bindings.push_back({"view.frame_selected", "Frame Selected", "View", GLFW_KEY_F});
    bindings.push_back({"view.frame_all",      "Frame All",      "View", GLFW_KEY_A});

    rebuildIndex();
}

void ShortcutManager::rebuildIndex() {
    actionIndex.clear();
    for (size_t i = 0; i < bindings.size(); i++) {
        actionIndex[bindings[i].actionId] = i;
    }
    // Collect all unique keys for state tracking
    prevKeyState.clear();
    curKeyState.clear();
    for (const auto& b : bindings) {
        prevKeyState[b.key] = false;
        curKeyState[b.key] = false;
    }
}

void ShortcutManager::updateKeyStates(GLFWwindow* window) {
    // Copy current to previous
    prevKeyState = curKeyState;

    // Poll modifier keys
    curCtrl = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
              glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    curShift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
               glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    curAlt = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
             glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

    // Poll all registered keys
    for (auto& [key, state] : curKeyState) {
        state = (glfwGetKey(window, key) == GLFW_PRESS);
    }
}

bool ShortcutManager::justPressed(const std::string& actionId) const {
    auto it = actionIndex.find(actionId);
    if (it == actionIndex.end()) return false;
    const KeyBinding& b = bindings[it->second];

    // Check modifier match (exact)
    if (b.ctrl != curCtrl || b.shift != curShift || b.alt != curAlt)
        return false;

    // Check key edge: currently pressed AND was not pressed last frame
    auto cur = curKeyState.find(b.key);
    auto prev = prevKeyState.find(b.key);
    if (cur == curKeyState.end() || prev == prevKeyState.end()) return false;
    return cur->second && !prev->second;
}

std::string ShortcutManager::getDisplayString(const std::string& actionId) const {
    auto it = actionIndex.find(actionId);
    if (it == actionIndex.end()) return "";
    const KeyBinding& b = bindings[it->second];
    std::string s;
    if (b.ctrl) s += "Ctrl+";
    if (b.shift) s += "Shift+";
    if (b.alt) s += "Alt+";
    s += keyToString(b.key);
    return s;
}

void ShortcutManager::rebind(const std::string& actionId, int key, bool ctrl, bool shift, bool alt) {
    auto it = actionIndex.find(actionId);
    if (it == actionIndex.end()) return;
    KeyBinding& b = bindings[it->second];
    b.key = key;
    b.ctrl = ctrl;
    b.shift = shift;
    b.alt = alt;
    rebuildIndex(); // refresh tracked keys
}

void ShortcutManager::saveToFile(const std::string& path) const {
    std::ofstream f(path);
    if (!f) return;
    for (const auto& b : bindings) {
        f << b.actionId << " " << b.key << " " << (b.ctrl ? 1 : 0)
          << " " << (b.shift ? 1 : 0) << " " << (b.alt ? 1 : 0) << "\n";
    }
}

void ShortcutManager::loadFromFile(const std::string& path) {
    std::ifstream f(path);
    if (!f) return;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ls(line);
        std::string actionId;
        int key, c, s, a;
        if (ls >> actionId >> key >> c >> s >> a) {
            auto it = actionIndex.find(actionId);
            if (it != actionIndex.end()) {
                KeyBinding& b = bindings[it->second];
                b.key = key;
                b.ctrl = (c != 0);
                b.shift = (s != 0);
                b.alt = (a != 0);
            }
        }
    }
    rebuildIndex();
}

std::string ShortcutManager::keyToString(int glfwKey) {
    switch (glfwKey) {
        case GLFW_KEY_A: return "A"; case GLFW_KEY_B: return "B"; case GLFW_KEY_C: return "C";
        case GLFW_KEY_D: return "D"; case GLFW_KEY_E: return "E"; case GLFW_KEY_F: return "F";
        case GLFW_KEY_G: return "G"; case GLFW_KEY_H: return "H"; case GLFW_KEY_I: return "I";
        case GLFW_KEY_J: return "J"; case GLFW_KEY_K: return "K"; case GLFW_KEY_L: return "L";
        case GLFW_KEY_M: return "M"; case GLFW_KEY_N: return "N"; case GLFW_KEY_O: return "O";
        case GLFW_KEY_P: return "P"; case GLFW_KEY_Q: return "Q"; case GLFW_KEY_R: return "R";
        case GLFW_KEY_S: return "S"; case GLFW_KEY_T: return "T"; case GLFW_KEY_U: return "U";
        case GLFW_KEY_V: return "V"; case GLFW_KEY_W: return "W"; case GLFW_KEY_X: return "X";
        case GLFW_KEY_Y: return "Y"; case GLFW_KEY_Z: return "Z";
        case GLFW_KEY_0: return "0"; case GLFW_KEY_1: return "1"; case GLFW_KEY_2: return "2";
        case GLFW_KEY_3: return "3"; case GLFW_KEY_4: return "4"; case GLFW_KEY_5: return "5";
        case GLFW_KEY_6: return "6"; case GLFW_KEY_7: return "7"; case GLFW_KEY_8: return "8";
        case GLFW_KEY_9: return "9";
        case GLFW_KEY_SPACE: return "Space"; case GLFW_KEY_ENTER: return "Enter";
        case GLFW_KEY_ESCAPE: return "Esc"; case GLFW_KEY_TAB: return "Tab";
        case GLFW_KEY_BACKSPACE: return "Backspace"; case GLFW_KEY_DELETE: return "Del";
        case GLFW_KEY_INSERT: return "Insert"; case GLFW_KEY_HOME: return "Home";
        case GLFW_KEY_END: return "End"; case GLFW_KEY_PAGE_UP: return "PgUp";
        case GLFW_KEY_PAGE_DOWN: return "PgDn";
        case GLFW_KEY_UP: return "Up"; case GLFW_KEY_DOWN: return "Down";
        case GLFW_KEY_LEFT: return "Left"; case GLFW_KEY_RIGHT: return "Right";
        case GLFW_KEY_F1: return "F1"; case GLFW_KEY_F2: return "F2";
        case GLFW_KEY_F3: return "F3"; case GLFW_KEY_F4: return "F4";
        case GLFW_KEY_F5: return "F5"; case GLFW_KEY_F6: return "F6";
        case GLFW_KEY_F7: return "F7"; case GLFW_KEY_F8: return "F8";
        case GLFW_KEY_F9: return "F9"; case GLFW_KEY_F10: return "F10";
        case GLFW_KEY_F11: return "F11"; case GLFW_KEY_F12: return "F12";
        case GLFW_KEY_MINUS: return "-"; case GLFW_KEY_EQUAL: return "=";
        case GLFW_KEY_LEFT_BRACKET: return "["; case GLFW_KEY_RIGHT_BRACKET: return "]";
        case GLFW_KEY_SEMICOLON: return ";"; case GLFW_KEY_APOSTROPHE: return "'";
        case GLFW_KEY_COMMA: return ","; case GLFW_KEY_PERIOD: return ".";
        case GLFW_KEY_SLASH: return "/"; case GLFW_KEY_BACKSLASH: return "\\";
        case GLFW_KEY_GRAVE_ACCENT: return "`";
        default: return "?";
    }
}

} // namespace opticsketch
