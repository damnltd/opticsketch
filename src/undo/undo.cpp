#include "undo/undo.h"
#include "scene/scene.h"

namespace opticsketch {

// --- Helper: exact snapshot (preserves ID) ---

static std::unique_ptr<Element> snapshotElement(const Element& e) {
    auto s = std::make_unique<Element>(e.type, e.id);
    s->label = e.label;
    s->transform = e.transform;
    s->locked = e.locked;
    s->visible = e.visible;
    s->layer = e.layer;
    s->boundsMin = e.boundsMin;
    s->boundsMax = e.boundsMax;
    s->meshVertices = e.meshVertices;
    s->meshSourcePath = e.meshSourcePath;
    return s;
}

static std::unique_ptr<Beam> snapshotBeam(const Beam& b) {
    auto s = std::make_unique<Beam>(b.id);
    s->label = b.label;
    s->start = b.start;
    s->end = b.end;
    s->color = b.color;
    s->width = b.width;
    s->visible = b.visible;
    s->layer = b.layer;
    return s;
}

// --- UndoStack ---

void UndoStack::push(std::unique_ptr<UndoCommand> cmd) {
    // Discard any commands after current index (redo history invalidated)
    if (index + 1 < static_cast<int>(commands.size())) {
        commands.erase(commands.begin() + index + 1, commands.end());
    }
    commands.push_back(std::move(cmd));
    index = static_cast<int>(commands.size()) - 1;
}

void UndoStack::undo(Scene& scene) {
    if (!canUndo()) return;
    commands[index]->undo(scene);
    --index;
}

void UndoStack::redo(Scene& scene) {
    if (!canRedo()) return;
    ++index;
    commands[index]->redo(scene);
}

bool UndoStack::canUndo() const {
    return index >= 0;
}

bool UndoStack::canRedo() const {
    return index + 1 < static_cast<int>(commands.size());
}

void UndoStack::clear() {
    commands.clear();
    index = -1;
}

// --- AddElementCmd ---

AddElementCmd::AddElementCmd(const Element& elem)
    : snapshot(snapshotElement(elem)), elementId(elem.id) {}

void AddElementCmd::undo(Scene& scene) {
    scene.removeElement(elementId);
}

void AddElementCmd::redo(Scene& scene) {
    scene.addElement(snapshotElement(*snapshot));
    scene.selectElement(elementId);
}

// --- RemoveElementCmd ---

RemoveElementCmd::RemoveElementCmd(const Element& elem)
    : snapshot(snapshotElement(elem)), elementId(elem.id) {}

void RemoveElementCmd::undo(Scene& scene) {
    scene.addElement(snapshotElement(*snapshot));
    scene.selectElement(elementId);
}

void RemoveElementCmd::redo(Scene& scene) {
    scene.removeElement(elementId);
}

// --- TransformElementCmd ---

TransformElementCmd::TransformElementCmd(const std::string& elemId, const Transform& oldT, const Transform& newT)
    : elementId(elemId), oldTransform(oldT), newTransform(newT) {}

void TransformElementCmd::undo(Scene& scene) {
    Element* e = scene.getElement(elementId);
    if (e) e->transform = oldTransform;
}

void TransformElementCmd::redo(Scene& scene) {
    Element* e = scene.getElement(elementId);
    if (e) e->transform = newTransform;
}

// --- AddBeamCmd ---

AddBeamCmd::AddBeamCmd(const Beam& beam)
    : snapshot(snapshotBeam(beam)), beamId(beam.id) {}

void AddBeamCmd::undo(Scene& scene) {
    scene.removeBeam(beamId);
}

void AddBeamCmd::redo(Scene& scene) {
    scene.addBeam(snapshotBeam(*snapshot));
    scene.selectBeam(beamId);
}

// --- RemoveBeamCmd ---

RemoveBeamCmd::RemoveBeamCmd(const Beam& beam)
    : snapshot(snapshotBeam(beam)), beamId(beam.id) {}

void RemoveBeamCmd::undo(Scene& scene) {
    scene.addBeam(snapshotBeam(*snapshot));
    scene.selectBeam(beamId);
}

void RemoveBeamCmd::redo(Scene& scene) {
    scene.removeBeam(beamId);
}

} // namespace opticsketch
