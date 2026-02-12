

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
        // Sources
        {"laser_hene", "HeNe Laser", "Sources", ElementType::Laser, ""},
        {"laser_diode", "Diode Laser", "Sources", ElementType::Laser, ""},
        {"laser_led", "LED Source", "Sources", ElementType::Laser, ""},
        {"fiber_output", "Fiber Output", "Fiber", ElementType::FiberCoupler, ""},
        // Mirrors
        {"mirror_flat_1in", "Flat Mirror 1\"", "Mirrors", ElementType::Mirror, ""},
        {"mirror_flat_2in", "Flat Mirror 2\"", "Mirrors", ElementType::Mirror, ""},
        {"mirror_concave", "Concave Mirror", "Mirrors", ElementType::Mirror, ""},
        {"mirror_convex", "Convex Mirror", "Mirrors", ElementType::Mirror, ""},
        // Lenses
        {"lens_pcx_25", "Plano-Convex 25mm", "Lenses", ElementType::Lens, ""},
        {"lens_pcx_50", "Plano-Convex 50mm", "Lenses", ElementType::Lens, ""},
        {"lens_pcx_100", "Plano-Convex 100mm", "Lenses", ElementType::Lens, ""},
        {"lens_pcc", "Plano-Concave", "Lenses", ElementType::Lens, ""},
        {"lens_biconvex", "Bi-Convex", "Lenses", ElementType::Lens, ""},
        // Beam Splitters
        {"bs_cube_5050", "50:50 Cube BS", "Beam Splitters", ElementType::BeamSplitter, ""},
        {"bs_plate_5050", "50:50 Plate BS", "Beam Splitters", ElementType::BeamSplitter, ""},
        {"bs_pbs_cube", "PBS Cube", "Beam Splitters", ElementType::BeamSplitter, ""},
        // Filters
        {"filter_nd", "ND Filter", "Filters", ElementType::Filter, ""},
        {"filter_polarizer", "Linear Polarizer", "Filters", ElementType::Filter, ""},
        {"filter_hwp", "Half-Wave Plate", "Filters", ElementType::Filter, ""},
        {"filter_qwp", "Quarter-Wave Plate", "Filters", ElementType::Filter, ""},
        // Apertures
        {"aperture_iris", "Iris Diaphragm", "Apertures", ElementType::Aperture, ""},
        {"aperture_pinhole", "Pinhole", "Apertures", ElementType::Aperture, ""},
        {"aperture_slit", "Single Slit", "Apertures", ElementType::Aperture, ""},
        // Prisms
        {"prism_equilateral", "Equilateral Prism", "Prisms", ElementType::Prism, ""},
        {"prism_rightangle", "Right-Angle Prism", "Prisms", ElementType::PrismRA, ""},
        // Detectors
        {"detector_photodiode", "Photodiode", "Detectors", ElementType::Detector, ""},
        {"detector_screen", "Camera/Screen", "Detectors", ElementType::Screen, ""},
        {"detector_powermeter", "Power Meter", "Detectors", ElementType::Detector, ""},
        // Mounts
        {"mount_post", "Optical Post", "Mounts", ElementType::Mount, ""},
        {"mount_mirror", "Mirror Mount", "Mounts", ElementType::Mount, ""},
    };
}

void LibraryPanel::render() {
    if (!visible) return;
    if (!ImGui::Begin("Element Library", &visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }
    
    // Category filter - tabs
    const char* categories[] = {"All", "Sources", "Mirrors", "Lenses", "Beam Splitters", "Filters", "Apertures", "Prisms", "Detectors", "Fiber", "Mounts", "Imported"};
    const int numCategories = 12;

    if (ImGui::BeginTabBar("##CategoryTabs", ImGuiTabBarFlags_FittingPolicyScroll | ImGuiTabBarFlags_TabListPopupButton)) {
        for (int i = 0; i < numCategories; i++) {
            if (ImGui::BeginTabItem(categories[i])) {
                selectedCategory = categories[i];
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }

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
        case ElementType::Filter:
            bgColor = ImVec4(0.15f, 0.1f, 0.2f, 0.6f);
            borderColor = ImVec4(0.6f, 0.4f, 0.8f, 0.8f);
            icon = "▮";
            break;
        case ElementType::Aperture:
            bgColor = ImVec4(0.2f, 0.15f, 0.1f, 0.6f);
            borderColor = ImVec4(0.8f, 0.6f, 0.3f, 0.8f);
            icon = "⊙";
            break;
        case ElementType::Prism:
            bgColor = ImVec4(0.1f, 0.15f, 0.18f, 0.6f);
            borderColor = ImVec4(0.5f, 0.8f, 0.9f, 0.8f);
            icon = "△";
            break;
        case ElementType::PrismRA:
            bgColor = ImVec4(0.1f, 0.15f, 0.18f, 0.6f);
            borderColor = ImVec4(0.5f, 0.8f, 0.9f, 0.8f);
            icon = "◣";
            break;
        case ElementType::Grating:
            bgColor = ImVec4(0.18f, 0.12f, 0.08f, 0.6f);
            borderColor = ImVec4(0.7f, 0.5f, 0.3f, 0.8f);
            icon = "≡";
            break;
        case ElementType::FiberCoupler:
            bgColor = ImVec4(0.2f, 0.12f, 0.05f, 0.6f);
            borderColor = ImVec4(1.0f, 0.6f, 0.2f, 0.8f);
            icon = "⊳";
            break;
        case ElementType::Screen:
            bgColor = ImVec4(0.1f, 0.2f, 0.1f, 0.6f);
            borderColor = ImVec4(0.3f, 0.8f, 0.3f, 0.8f);
            icon = "▭";
            break;
        case ElementType::Mount:
            bgColor = ImVec4(0.12f, 0.12f, 0.14f, 0.6f);
            borderColor = ImVec4(0.5f, 0.5f, 0.55f, 0.8f);
            icon = "┃";
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
    
    // Content overlay (drawn via drawList, no cursor reset needed)
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
