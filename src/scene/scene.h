#pragma once

#include "elements/element.h"
#include <vector>
#include <memory>
#include <string>

namespace opticsketch {

// Forward declaration
class Beam;

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
    
    // Beam management
    void addBeam(std::unique_ptr<Beam> beam);
    bool removeBeam(const std::string& id);
    Beam* getBeam(const std::string& id);
    const std::vector<std::unique_ptr<Beam>>& getBeams() const { return beams; }
    
    // Clear scene
    void clear();
    
    // Selection (mutual exclusion: selecting an element deselects beam and vice versa)
    void selectElement(const std::string& id);
    void selectBeam(const std::string& id);
    void deselectAll();
    Element* getSelectedElement() const { return selectedElement; }
    Beam* getSelectedBeam() const { return selectedBeam; }
    bool isSelected(const std::string& id) const;

private:
    std::vector<std::unique_ptr<Element>> elements;
    std::vector<std::unique_ptr<Beam>> beams;
    Element* selectedElement = nullptr;
    Beam* selectedBeam = nullptr;
};

} // namespace opticsketch
