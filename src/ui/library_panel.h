#pragma once

#include <string>
#include <vector>
#include <functional>
#include <imgui.h>
#include "elements/element.h"

namespace opticsketch {

struct LibraryItem {
    std::string id;
    std::string name;
    std::string category;
    ElementType type;
    std::string icon; // For future icon support
};

class LibraryPanel {
public:
    LibraryPanel();
    
    // Render the library panel UI
    void render();
    
    bool isVisible() const { return visible; }
    void setVisible(bool v) { visible = v; }
    
    // Set callback for when element is dragged/dropped
    void setOnElementDrag(std::function<void(ElementType, const std::string&)> callback) {
        onElementDrag = callback;
    }
    
private:
    bool visible = true;
    std::vector<LibraryItem> items;
    char searchText[256] = "";
    std::string selectedCategory = "All";
    float gridSize = 80.0f;
    
    std::function<void(ElementType, const std::string&)> onElementDrag;
    
    void loadBuiltinLibrary();
    void renderElementGrid();
    void renderElementItem(const LibraryItem& item);
    std::vector<LibraryItem> getFilteredItems() const;
};

} // namespace opticsketch
