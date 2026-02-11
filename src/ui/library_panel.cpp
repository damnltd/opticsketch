

#include "ui/library_panel.h"
#include "elements/basic_elements.h"
#include <algorithm>
#include <cstring>

namespace opticsketch {

LibraryPanel::LibraryPanel() {
    loadBuiltinLibrary();
}

void LibraryPanel::loadBuiltinLibrary() {
    items = {
        {"laser_generic", "Generic Laser", "Sources", ElementType::Laser, ""},
        {"mirror_flat", "Flat Mirror", "Mirrors", ElementType::Mirror, ""},
        {"lens_planoconvex", "Plano-Convex Lens", "Lenses", ElementType::Lens, ""},
        {"beamsplitter_cube", "50:50 Beam Splitter", "Beam Splitters", ElementType::BeamSplitter, ""},
        {"detector_photodiode", "Photodiode", "Detectors", ElementType::Detector, ""}
    };
}

void LibraryPanel::render() {
    if (!visible) return;
    if (!ImGui::Begin("Element Library", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }
    
    // Category tabs - Unity/Unreal style horizontal buttons
    const char* categories[] = {"All", "Sources", "Mirrors", "Lenses", "Beam Splitters", "Detectors", "Imported"};
    const int numCategories = 7;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));

    for (int i = 0; i < numCategories; i++) {
        if (i > 0) ImGui::SameLine();
        bool selected = (selectedCategory == categories[i]);
        
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        }
        
        if (ImGui::Button(categories[i])) {
            selectedCategory = categories[i];
        }
        
        ImGui::PopStyleColor(selected ? 2 : 1);
    }
    
    ImGui::PopStyleVar(2);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Search bar - full width
    ImGui::PushItemWidth(-1);
    ImGui::InputTextWithHint("##Search", "Search...", searchText, sizeof(searchText));
    ImGui::PopItemWidth();
    
    ImGui::Spacing();
    
    // Toolbar: Grid size slider and item count
    ImGui::BeginGroup();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    
    ImGui::SetNextItemWidth(120);
    ImGui::SliderFloat("##GridSize", &gridSize, 60.0f, 120.0f, "%.0f");
    ImGui::SameLine();
    
    ImGui::TextDisabled("%zu items", getFilteredItems().size());
    ImGui::PopStyleVar();
    ImGui::EndGroup();
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Element grid/list - scrollable area
    ImGui::BeginChild("##ElementScrollArea", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    renderElementGrid();
    ImGui::EndChild();
    
    ImGui::End();
}

// Removed - now using ImGui tabs in render()

void LibraryPanel::renderElementGrid() {
    auto filtered = getFilteredItems();
    
    // Grid layout - Unity/Unreal style
    float panelWidth = ImGui::GetContentRegionAvail().x;
    float spacing = 8.0f;
    int columns = std::max(1, static_cast<int>((panelWidth + spacing) / (gridSize + spacing)));
    
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(spacing, spacing));
    
    int col = 0;
    for (const auto& item : filtered) {
        if (col > 0) ImGui::SameLine();
        renderElementItem(item);
        col = (col + 1) % columns;
    }
    
    ImGui::PopStyleVar();
}

void LibraryPanel::renderElementItem(const LibraryItem& item) {
    // Get color and icon based on element type
    ImVec4 bgColor;
    ImVec4 borderColor;
    const char* icon = "";
    
    switch (item.type) {
        case ElementType::Laser:
            bgColor = ImVec4(0.2f, 0.1f, 0.1f, 0.6f);
            borderColor = ImVec4(1.0f, 0.3f, 0.3f, 0.8f);
            icon = "●";
            break;
        case ElementType::Mirror:
            bgColor = ImVec4(0.15f, 0.15f, 0.18f, 0.6f);
            borderColor = ImVec4(0.7f, 0.7f, 0.8f, 0.8f);
            icon = "◢";
            break;
        case ElementType::Lens:
            bgColor = ImVec4(0.1f, 0.15f, 0.2f, 0.6f);
            borderColor = ImVec4(0.4f, 0.7f, 1.0f, 0.8f);
            icon = "◯";
            break;
        case ElementType::BeamSplitter:
            bgColor = ImVec4(0.2f, 0.2f, 0.1f, 0.6f);
            borderColor = ImVec4(0.9f, 0.9f, 0.4f, 0.8f);
            icon = "◊";
            break;
        case ElementType::Detector:
            bgColor = ImVec4(0.1f, 0.2f, 0.1f, 0.6f);
            borderColor = ImVec4(0.3f, 1.0f, 0.3f, 0.8f);
            icon = "◉";
            break;
        case ElementType::ImportedMesh:
            bgColor = ImVec4(0.15f, 0.15f, 0.15f, 0.6f);
            borderColor = ImVec4(0.7f, 0.7f, 0.7f, 0.8f);
            icon = "▣";
            break;
    }
    
    // Grid view - Unity/Unreal style card
    ImVec2 cardSize(gridSize, gridSize);
    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImVec2 screenPos = ImGui::GetCursorScreenPos();
    
    // Card background with border
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool hovered = ImGui::IsMouseHoveringRect(screenPos, ImVec2(screenPos.x + cardSize.x, screenPos.y + cardSize.y));
    
    // Background
    drawList->AddRectFilled(screenPos, ImVec2(screenPos.x + cardSize.x, screenPos.y + cardSize.y), 
                            ImGui::ColorConvertFloat4ToU32(bgColor), 4.0f);
    
    // Border (thicker on hover)
    float borderWidth = hovered ? 2.0f : 1.0f;
    drawList->AddRect(screenPos, ImVec2(screenPos.x + cardSize.x, screenPos.y + cardSize.y),
                     ImGui::ColorConvertFloat4ToU32(borderColor), 4.0f, 0, borderWidth);
    
    // Use unique ID for each card to avoid conflicts
    std::string buttonId = "##Card_" + item.id;
    ImGui::SetCursorPos(cursorPos);
    
    // Invisible button for interaction
    if (ImGui::InvisibleButton(buttonId.c_str(), cardSize)) {
        // Clicked (but we use drag-drop, so this is just for interaction)
    }
    
    // Set up drag source - must be called after the button
    bool isDragging = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
        isDragging = true;
        // Find index of this item
        int itemIndex = -1;
        for (int idx = 0; idx < static_cast<int>(items.size()); idx++) {
            if (items[idx].id == item.id) { itemIndex = idx; break; }
        }
        ImGui::SetDragDropPayload("LIBRARY_ITEM", &itemIndex, sizeof(int));
        ImGui::Text("%s %s", icon, item.name.c_str());
        ImGui::EndDragDropSource();
    }
    
    // Content overlay
    ImGui::SetCursorPos(cursorPos);
    ImGui::PushClipRect(screenPos, ImVec2(screenPos.x + cardSize.x, screenPos.y + cardSize.y), true);
    
    // Icon - centered at top
    ImVec2 iconTextSize = ImGui::CalcTextSize(icon);
    ImVec2 iconPos(screenPos.x + (cardSize.x - iconTextSize.x) * 0.5f, 
                  screenPos.y + cardSize.y * 0.25f - iconTextSize.y * 0.5f);
    drawList->AddText(iconPos, ImGui::ColorConvertFloat4ToU32(borderColor), icon);
    
    // Name - centered at bottom
    ImVec2 nameTextSize = ImGui::CalcTextSize(item.name.c_str());
    float nameY = screenPos.y + cardSize.y * 0.75f;
    ImVec2 namePos(screenPos.x + (cardSize.x - nameTextSize.x) * 0.5f, nameY);
    
    // Truncate name if too long
    std::string displayName = item.name;
    if (nameTextSize.x > cardSize.x - 8) {
        displayName = displayName.substr(0, std::min(displayName.length(), size_t(15))) + "...";
        nameTextSize = ImGui::CalcTextSize(displayName.c_str());
        namePos.x = screenPos.x + (cardSize.x - nameTextSize.x) * 0.5f;
    }
    
    drawList->AddText(namePos, ImGui::GetColorU32(ImGuiCol_Text), displayName.c_str());
    
    ImGui::PopClipRect();
    
    // Tooltip - only show when hovering and not dragging
    if (ImGui::IsItemHovered() && !isDragging) {
        ImGui::BeginTooltip();
        ImGui::Text("%s %s", icon, item.name.c_str());
        ImGui::Separator();
        ImGui::Text("Category: %s", item.category.c_str());
        ImGui::Text("Drag to viewport to place");
        ImGui::EndTooltip();
    }
}

std::vector<LibraryItem> LibraryPanel::getFilteredItems() const {
    std::vector<LibraryItem> result;
    
    for (const auto& item : items) {
        // Category filter
        if (!selectedCategory.empty() && selectedCategory != "All" && item.category != selectedCategory) {
            continue;
        }
        
        // Search filter
        std::string searchStr(searchText);
        if (!searchStr.empty()) {
            std::string lowerName = item.name;
            std::string lowerSearch = searchStr;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::transform(lowerSearch.begin(), lowerSearch.end(), lowerSearch.begin(), ::tolower);
            
            if (lowerName.find(lowerSearch) == std::string::npos) {
                continue;
            }
        }
        
        result.push_back(item);
    }
    
    return result;
}

void LibraryPanel::addImportedItem(const std::string& name, const std::string& objPath) {
    // Generate unique id
    static int importCounter = 0;
    std::string id = "imported_" + std::to_string(++importCounter);

    LibraryItem item;
    item.id = id;
    item.name = name;
    item.category = "Imported";
    item.type = ElementType::ImportedMesh;
    item.icon = "";
    item.meshPath = objPath;
    items.push_back(std::move(item));
}

} // namespace opticsketch
