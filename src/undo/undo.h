#pragma once

#include <memory>
#include <vector>
#include <string>
#include <utility>
#include "elements/element.h"
#include "elements/annotation.h"
#include "render/beam.h"

namespace opticsketch {

class Scene;

// Base class for undoable commands
class UndoCommand {
public:
    virtual ~UndoCommand() = default;
    virtual void undo(Scene& scene) = 0;
    virtual void redo(Scene& scene) = 0;
};

// Undo/Redo stack
class UndoStack {
public:
    void push(std::unique_ptr<UndoCommand> cmd);
    void undo(Scene& scene);
    void redo(Scene& scene);
    bool canUndo() const;
    bool canRedo() const;
    void clear();
private:
    std::vector<std::unique_ptr<UndoCommand>> commands;
    int index = -1; // points to last executed command
};

// --- Concrete commands ---

// Add element (undo = remove, redo = re-add)
class AddElementCmd : public UndoCommand {
public:
    AddElementCmd(const Element& elem);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::unique_ptr<Element> snapshot;
    std::string elementId;
};

// Remove element (undo = re-add, redo = remove)
class RemoveElementCmd : public UndoCommand {
public:
    RemoveElementCmd(const Element& elem);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::unique_ptr<Element> snapshot;
    std::string elementId;
};

// Transform change (undo = restore old, redo = restore new)
class TransformElementCmd : public UndoCommand {
public:
    TransformElementCmd(const std::string& elemId, const Transform& oldT, const Transform& newT);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::string elementId;
    Transform oldTransform;
    Transform newTransform;
};

// Add beam (undo = remove, redo = re-add)
class AddBeamCmd : public UndoCommand {
public:
    AddBeamCmd(const Beam& beam);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::unique_ptr<Beam> snapshot;
    std::string beamId;
};

// Remove beam (undo = re-add, redo = remove)
class RemoveBeamCmd : public UndoCommand {
public:
    RemoveBeamCmd(const Beam& beam);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::unique_ptr<Beam> snapshot;
    std::string beamId;
};

// Add annotation (undo = remove, redo = re-add)
class AddAnnotationCmd : public UndoCommand {
public:
    AddAnnotationCmd(const Annotation& ann);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::unique_ptr<Annotation> snapshot;
    std::string annotationId;
};

// Remove annotation (undo = re-add, redo = remove)
class RemoveAnnotationCmd : public UndoCommand {
public:
    RemoveAnnotationCmd(const Annotation& ann);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::unique_ptr<Annotation> snapshot;
    std::string annotationId;
};

// Move annotation (undo = restore old pos, redo = restore new pos)
class MoveAnnotationCmd : public UndoCommand {
public:
    MoveAnnotationCmd(const std::string& annId, const glm::vec3& oldPos, const glm::vec3& newPos);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::string annotationId;
    glm::vec3 oldPosition;
    glm::vec3 newPosition;
};

// Compound command: wraps multiple sub-commands into one undo/redo unit
class CompoundUndoCmd : public UndoCommand {
public:
    void addCommand(std::unique_ptr<UndoCommand> cmd);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::vector<std::unique_ptr<UndoCommand>> cmds;
};

// Multi-element transform (undo/redo all transforms at once)
class MultiTransformCmd : public UndoCommand {
public:
    MultiTransformCmd(std::vector<std::pair<std::string, Transform>> oldTs,
                      std::vector<std::pair<std::string, Transform>> newTs);
    void undo(Scene& scene) override;
    void redo(Scene& scene) override;
private:
    std::vector<std::pair<std::string, Transform>> oldTransforms;
    std::vector<std::pair<std::string, Transform>> newTransforms;
};

} // namespace opticsketch
