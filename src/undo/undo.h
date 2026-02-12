#pragma once

#include <memory>
#include <vector>
#include <string>
#include "elements/element.h"
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

} // namespace opticsketch
