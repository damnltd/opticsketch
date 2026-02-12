#pragma once

#include "elements/element.h"
#include "camera/camera.h"
#include "scene/group.h"
#include <vector>
#include <memory>
#include <string>
#include <unordered_set>

namespace opticsketch {

// Forward declarations
class Beam;
class Annotation;
class Measurement;

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

    // Annotation management
    void addAnnotation(std::unique_ptr<Annotation> annotation);
    bool removeAnnotation(const std::string& id);
    Annotation* getAnnotation(const std::string& id);
    const std::vector<std::unique_ptr<Annotation>>& getAnnotations() const { return annotations; }

    // Measurement management
    void addMeasurement(std::unique_ptr<Measurement> measurement);
    bool removeMeasurement(const std::string& id);
    Measurement* getMeasurement(const std::string& id);
    const std::vector<std::unique_ptr<Measurement>>& getMeasurements() const { return measurements; }
    std::vector<Measurement*> getSelectedMeasurements() const;
    Measurement* getSelectedMeasurement() const;
    void selectMeasurement(const std::string& id, bool additive = false);

    // Clear scene
    void clear();

    // Remove all traced beams (beams where isTraced==true)
    void clearTracedBeams();

    // Selection — supports multi-select via unordered_set of IDs
    // additive=false clears selection first; additive=true adds to existing selection
    void selectElement(const std::string& id, bool additive = false);
    void selectBeam(const std::string& id, bool additive = false);
    void selectAnnotation(const std::string& id, bool additive = false);
    void toggleSelect(const std::string& id);
    void deselectAll();
    bool isSelected(const std::string& id) const;
    size_t getSelectionCount() const { return selectedIds.size(); }

    // Get all selected elements/beams/annotations
    std::vector<Element*> getSelectedElements() const;
    std::vector<Beam*> getSelectedBeams() const;
    std::vector<Annotation*> getSelectedAnnotations() const;
    Annotation* getSelectedAnnotation() const;

    // Backward compat: returns first selected element/beam or nullptr
    Element* getSelectedElement() const;
    Beam* getSelectedBeam() const;

    // Select all
    void selectAll();

    // Group management
    void addGroup(const Group& group);
    bool removeGroup(const std::string& groupId);
    Group* getGroup(const std::string& groupId);
    const std::vector<Group>& getGroups() const { return groups; }
    Group* findGroupContaining(const std::string& objectId);
    Group createGroupFromSelection();
    void dissolveGroup(const std::string& groupId);
    void selectGroupMembers(const std::string& groupId, bool additive = false);

    // View presets
    void addViewPreset(const ViewPreset& preset);
    void removeViewPreset(size_t index);
    const std::vector<ViewPreset>& getViewPresets() const { return viewPresets; }

private:
    std::vector<std::unique_ptr<Element>> elements;
    std::vector<std::unique_ptr<Beam>> beams;
    std::vector<std::unique_ptr<Annotation>> annotations;
    std::vector<std::unique_ptr<Measurement>> measurements;
    std::unordered_set<std::string> selectedIds;
    std::vector<Group> groups;
    std::vector<ViewPreset> viewPresets;
};

} // namespace opticsketch
