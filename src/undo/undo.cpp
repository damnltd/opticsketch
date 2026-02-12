#include "undo/undo.h"
#include "scene/scene.h"
#include "elements/annotation.h"
#include "elements/measurement.h"

namespace opticsketch {

// --- Helper: exact snapshot (preserves ID) ---

static std::unique_ptr<Element> snapshotElement(const Element& e) {
    auto s = std::make_unique<Element>(e.type, e.id);
    s->label = e.label;
    s->transform = e.transform;
    s->locked = e.locked;
    s->visible = e.visible;
    s->showLabel = e.showLabel;
    s->layer = e.layer;
    s->boundsMin = e.boundsMin;
    s->boundsMax = e.boundsMax;
    s->optics = e.optics;
    s->material = e.material;
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
    s->isTraced = b.isTraced;
    s->sourceElementId = b.sourceElementId;
    s->isGaussian = b.isGaussian;
    s->waistW0 = b.waistW0;
    s->wavelength = b.wavelength;
    s->waistPosition = b.waistPosition;
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

// --- Helper: snapshot annotation ---

static std::unique_ptr<Annotation> snapshotAnnotation(const Annotation& a) {
    auto s = std::make_unique<Annotation>(a.id);
    s->label = a.label;
    s->text = a.text;
    s->position = a.position;
    s->color = a.color;
    s->fontSize = a.fontSize;
    s->visible = a.visible;
    s->layer = a.layer;
    return s;
}

// --- AddAnnotationCmd ---

AddAnnotationCmd::AddAnnotationCmd(const Annotation& ann)
    : snapshot(snapshotAnnotation(ann)), annotationId(ann.id) {}

void AddAnnotationCmd::undo(Scene& scene) {
    scene.removeAnnotation(annotationId);
}

void AddAnnotationCmd::redo(Scene& scene) {
    scene.addAnnotation(snapshotAnnotation(*snapshot));
    scene.selectAnnotation(annotationId);
}

// --- RemoveAnnotationCmd ---

RemoveAnnotationCmd::RemoveAnnotationCmd(const Annotation& ann)
    : snapshot(snapshotAnnotation(ann)), annotationId(ann.id) {}

void RemoveAnnotationCmd::undo(Scene& scene) {
    scene.addAnnotation(snapshotAnnotation(*snapshot));
    scene.selectAnnotation(annotationId);
}

void RemoveAnnotationCmd::redo(Scene& scene) {
    scene.removeAnnotation(annotationId);
}

// --- MoveAnnotationCmd ---

MoveAnnotationCmd::MoveAnnotationCmd(const std::string& annId, const glm::vec3& oldPos, const glm::vec3& newPos)
    : annotationId(annId), oldPosition(oldPos), newPosition(newPos) {}

void MoveAnnotationCmd::undo(Scene& scene) {
    Annotation* a = scene.getAnnotation(annotationId);
    if (a) a->position = oldPosition;
}

void MoveAnnotationCmd::redo(Scene& scene) {
    Annotation* a = scene.getAnnotation(annotationId);
    if (a) a->position = newPosition;
}

// --- CompoundUndoCmd ---

void CompoundUndoCmd::addCommand(std::unique_ptr<UndoCommand> cmd) {
    cmds.push_back(std::move(cmd));
}

void CompoundUndoCmd::undo(Scene& scene) {
    for (int i = static_cast<int>(cmds.size()) - 1; i >= 0; --i)
        cmds[i]->undo(scene);
}

void CompoundUndoCmd::redo(Scene& scene) {
    for (auto& cmd : cmds)
        cmd->redo(scene);
}

// --- MultiTransformCmd ---

MultiTransformCmd::MultiTransformCmd(std::vector<std::pair<std::string, Transform>> oldTs,
                                     std::vector<std::pair<std::string, Transform>> newTs)
    : oldTransforms(std::move(oldTs)), newTransforms(std::move(newTs)) {}

void MultiTransformCmd::undo(Scene& scene) {
    for (auto& [id, t] : oldTransforms) {
        Element* e = scene.getElement(id);
        if (e) e->transform = t;
    }
}

void MultiTransformCmd::redo(Scene& scene) {
    for (auto& [id, t] : newTransforms) {
        Element* e = scene.getElement(id);
        if (e) e->transform = t;
    }
}

// --- Helper: snapshot measurement ---

static std::unique_ptr<Measurement> snapshotMeasurement(const Measurement& m) {
    auto s = std::make_unique<Measurement>(m.id);
    s->label = m.label;
    s->startPoint = m.startPoint;
    s->endPoint = m.endPoint;
    s->color = m.color;
    s->fontSize = m.fontSize;
    s->visible = m.visible;
    s->layer = m.layer;
    return s;
}

// --- AddMeasurementCmd ---

AddMeasurementCmd::AddMeasurementCmd(const Measurement& meas)
    : snapshot(snapshotMeasurement(meas)), measurementId(meas.id) {}

void AddMeasurementCmd::undo(Scene& scene) {
    scene.removeMeasurement(measurementId);
}

void AddMeasurementCmd::redo(Scene& scene) {
    scene.addMeasurement(snapshotMeasurement(*snapshot));
    scene.selectMeasurement(measurementId);
}

// --- RemoveMeasurementCmd ---

RemoveMeasurementCmd::RemoveMeasurementCmd(const Measurement& meas)
    : snapshot(snapshotMeasurement(meas)), measurementId(meas.id) {}

void RemoveMeasurementCmd::undo(Scene& scene) {
    scene.addMeasurement(snapshotMeasurement(*snapshot));
    scene.selectMeasurement(measurementId);
}

void RemoveMeasurementCmd::redo(Scene& scene) {
    scene.removeMeasurement(measurementId);
}

// --- CreateGroupCmd ---

CreateGroupCmd::CreateGroupCmd(const Group& group) : snapshot(group) {}

void CreateGroupCmd::undo(Scene& scene) {
    scene.dissolveGroup(snapshot.id);
}

void CreateGroupCmd::redo(Scene& scene) {
    scene.addGroup(snapshot);
}

// --- DissolveGroupCmd ---

DissolveGroupCmd::DissolveGroupCmd(const Group& group) : snapshot(group) {}

void DissolveGroupCmd::undo(Scene& scene) {
    scene.addGroup(snapshot);
}

void DissolveGroupCmd::redo(Scene& scene) {
    scene.dissolveGroup(snapshot.id);
}

} // namespace opticsketch
