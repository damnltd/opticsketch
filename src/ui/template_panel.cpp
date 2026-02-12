#include "ui/template_panel.h"
#include "templates/templates.h"
#include "scene/scene.h"
#include "undo/undo.h"
#include <GLFW/glfw3.h>

namespace opticsketch {

void TemplatePanel::render(Scene* scene, UndoStack* undoStack,
                           std::string* projectPath, GLFWwindow* window) {
    if (!visible) return;

    if (!ImGui::Begin("Templates", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    const auto& templates = getTemplateList();

    // Category filter
    // Collect unique categories
    const char* categories[] = {"All", "Interferometers", "Imaging", "Spectroscopy", "Fiber", "Polarimetry"};
    int catCount = sizeof(categories) / sizeof(categories[0]);

    if (ImGui::BeginCombo("Category", selectedCategory.c_str())) {
        for (int i = 0; i < catCount; i++) {
            bool selected = (selectedCategory == categories[i]);
            if (ImGui::Selectable(categories[i], selected))
                selectedCategory = categories[i];
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Template list
    for (const auto& t : templates) {
        if (selectedCategory != "All" && t.category != selectedCategory)
            continue;

        ImGui::PushID(t.id.c_str());

        // Card-style: colored header bar + description
        ImVec2 avail = ImGui::GetContentRegionAvail();

        ImGui::BeginGroup();
        // Template name (bold via color highlight)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.9f, 1.0f, 1.0f));
        ImGui::Text("%s", t.name.c_str());
        ImGui::PopStyleColor();

        // Description (dimmed)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.65f, 1.0f));
        ImGui::TextWrapped("%s", t.description.c_str());
        ImGui::PopStyleColor();

        // Category tag
        ImGui::SameLine(avail.x - 60.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.6f, 0.7f, 1.0f));
        ImGui::Text("[%s]", t.category.c_str());
        ImGui::PopStyleColor();

        ImGui::EndGroup();

        // Make the whole area clickable
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
            pendingTemplateId = t.id;
            confirmOpen = true;
        }

        // Hover highlight
        if (ImGui::IsItemHovered()) {
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(min.x - 4, min.y - 2), ImVec2(max.x + 4, max.y + 2),
                IM_COL32(80, 120, 200, 30), 4.0f);
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    // Confirmation dialog
    if (confirmOpen) {
        ImGui::OpenPopup("Load Template?");
        confirmOpen = false;
    }

    if (ImGui::BeginPopupModal("Load Template?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Find the template name for display
        std::string templateName = pendingTemplateId;
        for (const auto& t : templates) {
            if (t.id == pendingTemplateId) {
                templateName = t.name;
                break;
            }
        }
        ImGui::Text("Load \"%s\"?", templateName.c_str());
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "The current scene will be replaced.");
        ImGui::Spacing();

        float buttonWidth = 100.0f;
        float spacing = 10.0f;
        float totalWidth = buttonWidth * 2 + spacing;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f);

        if (ImGui::Button("Load", ImVec2(buttonWidth, 0))) {
            loadTemplate(pendingTemplateId, scene);
            if (undoStack) undoStack->clear();
            if (projectPath) projectPath->clear();
            if (window) glfwSetWindowTitle(window, "OpticSketch - Untitled");
            pendingTemplateId.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, spacing);
        if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
            pendingTemplateId.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace opticsketch
