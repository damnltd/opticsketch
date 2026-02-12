#include "ui/shortcuts_panel.h"
#include "input/shortcut_manager.h"
#include <GLFW/glfw3.h>

namespace opticsketch {

void ShortcutsPanel::render(ShortcutManager* mgr) {
    if (!visible || !mgr) return;

    if (!ImGui::Begin("Keyboard Shortcuts", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    // Rebind modal
    if (rebinding) {
        ImGui::OpenPopup("Rebind Key");
        rebinding = false;
    }

    if (ImGui::BeginPopupModal("Rebind Key", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Press a key combination for:");
        ImGui::TextColored(ImVec4(1, 1, 0.5f, 1), "%s", rebindActionId.c_str());
        ImGui::Spacing();
        ImGui::TextDisabled("Press a non-modifier key (with optional Ctrl/Shift/Alt)");
        ImGui::TextDisabled("Press Escape to cancel");

        // Detect key press via ImGui IO (works even in modal)
        ImGuiIO& io = ImGui::GetIO();
        for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; k++) {
            // Skip modifier keys themselves
            if (k == GLFW_KEY_LEFT_SHIFT || k == GLFW_KEY_RIGHT_SHIFT ||
                k == GLFW_KEY_LEFT_CONTROL || k == GLFW_KEY_RIGHT_CONTROL ||
                k == GLFW_KEY_LEFT_ALT || k == GLFW_KEY_RIGHT_ALT ||
                k == GLFW_KEY_LEFT_SUPER || k == GLFW_KEY_RIGHT_SUPER)
                continue;

            if (ImGui::IsKeyPressed((ImGuiKey)k, false)) {
                if (k == GLFW_KEY_ESCAPE) {
                    ImGui::CloseCurrentPopup();
                    break;
                }
                bool ctrl = io.KeyCtrl;
                bool shift = io.KeyShift;
                bool alt = io.KeyAlt;
                mgr->rebind(rebindActionId, k, ctrl, shift, alt);
                ImGui::CloseCurrentPopup();
                break;
            }
        }
        ImGui::EndPopup();
    }

    // Shortcuts table
    const auto& bindings = mgr->getBindings();
    std::string lastCategory;

    if (ImGui::BeginTable("shortcuts", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_None, 2.0f);
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_None, 1.5f);
        ImGui::TableSetupColumn("##rebind", ImGuiTableColumnFlags_None, 0.5f);
        ImGui::TableHeadersRow();

        for (const auto& b : bindings) {
            if (b.category != lastCategory) {
                lastCategory = b.category;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(ImVec4(0.7f, 0.8f, 1.0f, 1.0f), "%s", b.category.c_str());
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", b.displayName.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", mgr->getDisplayString(b.actionId).c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(b.actionId.c_str());
            if (ImGui::SmallButton("Rebind")) {
                rebindActionId = b.actionId;
                rebinding = true;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::Spacing();
    if (ImGui::Button("Reset All")) {
        mgr->registerDefaults();
    }

    ImGui::End();
}

} // namespace opticsketch
