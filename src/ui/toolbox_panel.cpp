#include "ui/toolbox_panel.h"
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

namespace opticsketch {

static const float ICON_PAD = 8.f;
static const float BUTTON_SIZE = 44.f;

static void drawSelectIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    ImVec2 c = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float r = (std::min(max.x - min.x, max.y - min.y) - ICON_PAD) * 0.35f;
    // Cursor/arrow: small circle + arrow from top-left
    drawList->AddCircle(c, r, IM_COL32(255, 255, 255, 255), 0, 1.8f);
    drawList->AddLine(ImVec2(c.x - r * 1.4f, c.y - r * 1.4f), ImVec2(c.x - r * 0.3f, c.y - r * 0.3f), IM_COL32(255, 255, 255, 255), 1.8f);
    drawList->AddLine(ImVec2(c.x - r * 0.3f, c.y - r * 0.3f), ImVec2(c.x + r * 0.4f, c.y - r * 0.1f), IM_COL32(255, 255, 255, 255), 1.8f);
    drawList->AddLine(ImVec2(c.x - r * 0.3f, c.y - r * 0.3f), ImVec2(c.x - r * 0.1f, c.y + r * 0.4f), IM_COL32(255, 255, 255, 255), 1.8f);
}

static void drawMoveIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    ImVec2 c = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float s = (std::min(max.x - min.x, max.y - min.y) - ICON_PAD) * 0.32f;
    ImU32 col = IM_COL32(255, 255, 255, 255);
    float th = 1.6f;
    // Three axes: X (right), Y (up), Z (out = shorter)
    drawList->AddLine(ImVec2(c.x - s, c.y), ImVec2(c.x + s, c.y), col, th);
    drawList->AddLine(ImVec2(c.x, c.y + s), ImVec2(c.x, c.y - s), col, th);
    drawList->AddLine(ImVec2(c.x, c.y), ImVec2(c.x + s * 0.6f, c.y - s * 0.6f), col, th * 0.9f);
    // Arrowheads
    drawList->AddLine(ImVec2(c.x + s, c.y), ImVec2(c.x + s - 5, c.y - 4), col, th);
    drawList->AddLine(ImVec2(c.x + s, c.y), ImVec2(c.x + s - 5, c.y + 4), col, th);
    drawList->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x - 4, c.y - s + 5), col, th);
    drawList->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x + 4, c.y - s + 5), col, th);
}

static void drawRotateIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    ImVec2 c = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float r = (std::min(max.x - min.x, max.y - min.y) - ICON_PAD) * 0.38f;
    ImU32 col = IM_COL32(255, 255, 255, 255);
    // Arc (about 3/4 of a circle) + arrow at end
    const int segments = 24;
    for (int i = 0; i < segments; i++) {
        float a0 = -0.85f * 3.14159f + (2.6f * 3.14159f) * (float)i / (float)segments;
        float a1 = -0.85f * 3.14159f + (2.6f * 3.14159f) * (float)(i + 1) / (float)segments;
        drawList->AddLine(
            ImVec2(c.x + r * cosf(a0), c.y - r * sinf(a0)),
            ImVec2(c.x + r * cosf(a1), c.y - r * sinf(a1)),
            col, 1.8f);
    }
    float ae = -0.85f * 3.14159f + 2.6f * 3.14159f;
    ImVec2 tip(c.x + r * cosf(ae), c.y - r * sinf(ae));
    drawList->AddLine(tip, ImVec2(tip.x - 6, tip.y + 4), col, 1.8f);
    drawList->AddLine(tip, ImVec2(tip.x - 2, tip.y + 7), col, 1.8f);
}

static void drawScaleIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    ImVec2 c = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float s = (std::min(max.x - min.x, max.y - min.y) - ICON_PAD) * 0.32f;
    ImU32 col = IM_COL32(255, 255, 255, 255);
    float th = 1.6f;
    // Small inner square + larger outer square (scale from center)
    drawList->AddRect(ImVec2(c.x - s * 0.5f, c.y - s * 0.5f), ImVec2(c.x + s * 0.5f, c.y + s * 0.5f), col, 0.f, 0, th);
    drawList->AddRect(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), col, 0.f, 0, th);
    // Corner ticks
    drawList->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x - s - 4, c.y - s), col, th);
    drawList->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x - s, c.y - s - 4), col, th);
    drawList->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s + 4, c.y - s), col, th);
    drawList->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s, c.y - s - 4), col, th);
    drawList->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x - s - 4, c.y + s), col, th);
    drawList->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x - s, c.y + s + 4), col, th);
    drawList->AddLine(ImVec2(c.x + s, c.y + s), ImVec2(c.x + s + 4, c.y + s), col, th);
    drawList->AddLine(ImVec2(c.x + s, c.y + s), ImVec2(c.x + s, c.y + s + 4), col, th);
}

static void drawBeamIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    ImVec2 c = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float s = (std::min(max.x - min.x, max.y - min.y) - ICON_PAD) * 0.35f;
    ImU32 col = IM_COL32(255, 100, 100, 255); // Red color for beam
    float th = 2.0f;
    // Diagonal line (beam) with arrowhead
    ImVec2 start(c.x - s * 0.7f, c.y + s * 0.7f);
    ImVec2 end(c.x + s * 0.7f, c.y - s * 0.7f);
    drawList->AddLine(start, end, col, th);
    // Arrowhead
    glm::vec2 dir(end.x - start.x, end.y - start.y);
    float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
    if (len > 1e-5f) {
        dir.x /= len;
        dir.y /= len;
        ImVec2 perp(-dir.y, dir.x);
        ImVec2 tip = end;
        ImVec2 base1(end.x - dir.x * 6 - perp.x * 4, end.y - dir.y * 6 - perp.y * 4);
        ImVec2 base2(end.x - dir.x * 6 + perp.x * 4, end.y - dir.y * 6 + perp.y * 4);
        drawList->AddTriangleFilled(tip, base1, base2, col);
    }
}

static void drawAnnotationIcon(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    ImVec2 c = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float s = (std::min(max.x - min.x, max.y - min.y) - ICON_PAD) * 0.35f;
    ImU32 col = IM_COL32(240, 240, 200, 255);
    float th = 1.8f;
    // "T" letter icon for text annotation
    drawList->AddLine(ImVec2(c.x - s * 0.7f, c.y - s), ImVec2(c.x + s * 0.7f, c.y - s), col, th);
    drawList->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x, c.y + s), col, th);
    // Underline
    drawList->AddLine(ImVec2(c.x - s * 0.4f, c.y + s), ImVec2(c.x + s * 0.4f, c.y + s), col, th);
}

ToolboxPanel::ToolboxPanel() {
}

void ToolboxPanel::render() {
    if (!visible) return;
    
    ImGui::Begin("Toolbox", &visible, ImGuiWindowFlags_None);
    
    ImGui::Text("Tools");
    ImGui::Separator();
    ImGui::Spacing();
    
    renderToolButtons();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    const char* toolName = "";
    const char* toolDesc = "";
    switch (currentTool) {
        case ToolMode::Select: toolName = "Select"; toolDesc = "Click to select objects (Q)"; break;
        case ToolMode::Move:   toolName = "Move";   toolDesc = "Click and drag to move (W)"; break;
        case ToolMode::Rotate: toolName = "Rotate"; toolDesc = "Click and drag to rotate (E)"; break;
        case ToolMode::Scale: toolName = "Scale";  toolDesc = "Click and drag to scale (R)"; break;
        case ToolMode::DrawBeam: toolName = "Draw Beam"; toolDesc = "Click to place beam points (B)"; break;
        case ToolMode::PlaceAnnotation: toolName = "Annotation"; toolDesc = "Click to place text annotation (T)"; break;
    }
    ImGui::Text("%s", toolName);
    ImGui::TextDisabled("%s", toolDesc);
    
    ImGui::End();
}

void ToolboxPanel::renderToolButtons() {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    auto drawToolButton = [&](const char* id, ToolMode mode, void (*drawIcon)(ImDrawList*, ImVec2, ImVec2)) {
        bool active = (currentTool == mode);
        if (active) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        }
        if (ImGui::Button(id, ImVec2(BUTTON_SIZE, BUTTON_SIZE))) {
            currentTool = mode;
        }
        if (active) {
            ImGui::PopStyleColor();
        }
        ImVec2 bmin = ImGui::GetItemRectMin();
        ImVec2 bmax = ImGui::GetItemRectMax();
        drawIcon(drawList, bmin, bmax);
    };
    
    drawToolButton("##ToolSelect", ToolMode::Select, drawSelectIcon);
    ImGui::SameLine();
    drawToolButton("##ToolMove", ToolMode::Move, drawMoveIcon);
    ImGui::SameLine();
    drawToolButton("##ToolRotate", ToolMode::Rotate, drawRotateIcon);
    ImGui::SameLine();
    drawToolButton("##ToolScale", ToolMode::Scale, drawScaleIcon);
    ImGui::SameLine();
    drawToolButton("##ToolDrawBeam", ToolMode::DrawBeam, drawBeamIcon);
    ImGui::SameLine();
    drawToolButton("##ToolAnnotation", ToolMode::PlaceAnnotation, drawAnnotationIcon);

    ImGui::PopStyleVar(2);
}

} // namespace opticsketch
