#pragma once

#include "elements/element.h"
#include <vector>
#include <memory>
#include <string>

namespace opticsketch {

class Scene {
public:
    Scene();
    ~Scene();
    
    // Add element to scene
    void addElement(std::unique_ptr<Element> element);
    
    // Remove element by ID
    bool removeElement(const std::string& id);
    
    // Get element by ID
    Element* getElement(const std::string& id);
    
    // Get all elements
    const std::vector<std::unique_ptr<Element>>& getElements() const { return elements; }
    
    // Clear scene
    void clear();
    
    // Selection
    void selectElement(const std::string& id);
    void deselectAll();
    Element* getSelectedElement() const { return selectedElement; }
    bool isSelected(const std::string& id) const;
    
private:
    std::vector<std::unique_ptr<Element>> elements;
    Element* selectedElement = nullptr;
};

} // namespace opticsketch
