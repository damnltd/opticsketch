#include "templates/templates.h"
#include "scene/scene.h"
#include "elements/basic_elements.h"
#include "render/beam.h"
#include "elements/annotation.h"
#include <glm/gtc/quaternion.hpp>

namespace opticsketch {

// Helper: create element, position it, label it, add to scene
static Element* placeElement(Scene* scene, ElementType type,
                             const glm::vec3& pos, const std::string& label,
                             const glm::quat& rot = glm::quat(1, 0, 0, 0)) {
    auto elem = createElement(type);
    elem->transform.position = pos;
    elem->transform.rotation = rot;
    elem->label = label;
    elem->showLabel = true;
    scene->addElement(std::move(elem));
    return scene->getElements().back().get();
}

// Helper: add beam between two points
static void addBeam(Scene* scene, const glm::vec3& start, const glm::vec3& end,
                    const glm::vec3& color = glm::vec3(1.0f, 0.0f, 0.0f), float width = 2.0f) {
    auto beam = std::make_unique<Beam>();
    beam->start = start;
    beam->end = end;
    beam->color = color;
    beam->width = width;
    scene->addBeam(std::move(beam));
}

// Helper: add title annotation above the scene
static void addTitle(Scene* scene, const std::string& text, const glm::vec3& pos) {
    auto ann = std::make_unique<Annotation>();
    ann->text = text;
    ann->label = text;
    ann->position = pos;
    ann->fontSize = 18.0f;
    scene->addAnnotation(std::move(ann));
}

// 45-degree rotation around Y axis (for beam splitters, mirrors at 45 deg)
static glm::quat rotY45() {
    return glm::angleAxis(glm::radians(45.0f), glm::vec3(0.0f, 1.0f, 0.0f));
}

static glm::quat rotY(float degrees) {
    return glm::angleAxis(glm::radians(degrees), glm::vec3(0.0f, 1.0f, 0.0f));
}

// ── Template builders ──────────────────────────────────────────────

static void buildMichelson(Scene* scene) {
    // Laser → BS (45°) → Mirror1 (reflected arm, +X)
    //                   → Mirror2 (transmitted arm, +Z)
    //         BS recombines → Detector (-X)
    placeElement(scene, ElementType::Laser,        {0, 0, -10}, "Laser");
    placeElement(scene, ElementType::BeamSplitter,  {0, 0,   0}, "Beam Splitter", rotY45());
    placeElement(scene, ElementType::Mirror,        {8, 0,   0}, "Mirror 1", rotY(90));
    placeElement(scene, ElementType::Mirror,        {0, 0,   8}, "Mirror 2");
    placeElement(scene, ElementType::Detector,      {-8, 0,  0}, "Detector", rotY(90));

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    addBeam(scene, {0, 0, -8},  {0, 0, 0},   red);  // Laser → BS
    addBeam(scene, {0, 0,  0},  {6, 0, 0},   red);  // BS → Mirror1
    addBeam(scene, {6, 0,  0},  {0, 0, 0},   red);  // Mirror1 → BS
    addBeam(scene, {0, 0,  0},  {0, 0, 6},   red);  // BS → Mirror2
    addBeam(scene, {0, 0,  6},  {0, 0, 0},   red);  // Mirror2 → BS
    addBeam(scene, {0, 0,  0},  {-6, 0, 0},  red);  // BS → Detector

    addTitle(scene, "Michelson Interferometer", {0, 3, -2});
}

static void buildMachZehnder(Scene* scene) {
    // Laser → BS1 → Mirror1 (upper arm, +X then +Z)
    //              → Mirror2 (lower arm, +Z then +X)
    //         BS2 recombines → Detector
    placeElement(scene, ElementType::Laser,         {-12, 0, -6}, "Laser");
    placeElement(scene, ElementType::BeamSplitter,  { -4, 0, -6}, "BS 1", rotY45());
    placeElement(scene, ElementType::Mirror,        {  4, 0, -6}, "Mirror 1", rotY45());
    placeElement(scene, ElementType::Mirror,        { -4, 0,  4}, "Mirror 2", rotY45());
    placeElement(scene, ElementType::BeamSplitter,  {  4, 0,  4}, "BS 2", rotY45());
    placeElement(scene, ElementType::Detector,      { 12, 0,  4}, "Detector", rotY(90));

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    addBeam(scene, {-10, 0, -6}, {-4, 0, -6},  red);  // Laser → BS1
    addBeam(scene, { -4, 0, -6}, { 4, 0, -6},  red);  // BS1 → Mirror1 (upper)
    addBeam(scene, {  4, 0, -6}, { 4, 0,  4},  red);  // Mirror1 → BS2
    addBeam(scene, { -4, 0, -6}, {-4, 0,  4},  red);  // BS1 → Mirror2 (lower)
    addBeam(scene, { -4, 0,  4}, { 4, 0,  4},  red);  // Mirror2 → BS2
    addBeam(scene, {  4, 0,  4}, {10, 0,  4},  red);  // BS2 → Detector

    addTitle(scene, "Mach-Zehnder Interferometer", {0, 3, -8});
}

static void buildFabryPerot(Scene* scene) {
    // Laser → Mirror1 → (cavity) → Mirror2 → Detector (all inline along Z)
    placeElement(scene, ElementType::Laser,    {0, 0, -14}, "Laser");
    placeElement(scene, ElementType::Mirror,   {0, 0,  -4}, "Mirror 1 (R=99%)");
    placeElement(scene, ElementType::Mirror,   {0, 0,   4}, "Mirror 2 (R=99%)");
    placeElement(scene, ElementType::Detector, {0, 0,  14}, "Detector");

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    addBeam(scene, {0, 0, -12}, {0, 0, -4},  red);  // Laser → M1
    addBeam(scene, {0, 0,  -4}, {0, 0,  4},  red);  // M1 → M2 (cavity)
    addBeam(scene, {0, 0,   4}, {0, 0, 12},  red);  // M2 → Detector

    addTitle(scene, "Fabry-Perot Cavity", {0, 3, -6});
}

static void buildBeamExpander(Scene* scene) {
    // Laser → negative lens (diverge) → positive lens (collimate)
    placeElement(scene, ElementType::Laser, {0, 0, -14}, "Laser");
    placeElement(scene, ElementType::Lens,  {0, 0,  -4}, "Diverging Lens (f=-25mm)");
    placeElement(scene, ElementType::Lens,  {0, 0,   6}, "Collimating Lens (f=75mm)");

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    // Narrow input beam
    addBeam(scene, {0, 0, -12}, {0, 0, -4}, red);
    // Diverging cone (approx with 3 rays)
    addBeam(scene, {0,    0, -4}, { 0,    0, 6}, red);
    addBeam(scene, {0,  0.1, -4}, { 0,  0.4, 6}, red, 1.5f);
    addBeam(scene, {0, -0.1, -4}, { 0, -0.4, 6}, red, 1.5f);
    // Expanded collimated output
    addBeam(scene, { 0,    0, 6}, { 0,    0, 16}, red);
    addBeam(scene, { 0,  0.4, 6}, { 0,  0.4, 16}, red, 1.5f);
    addBeam(scene, { 0, -0.4, 6}, { 0, -0.4, 16}, red, 1.5f);

    addTitle(scene, "Beam Expander (3x Galilean)", {0, 3, -6});
}

static void build4fSystem(Scene* scene) {
    // Laser → Lens1 → (focus) → Aperture (Fourier plane) → Lens2 → Screen
    float f = 5.0f; // focal length
    placeElement(scene, ElementType::Laser,    {0, 0, -16},     "Laser");
    placeElement(scene, ElementType::Lens,     {0, 0, -f},      "Lens 1 (f=50mm)");
    placeElement(scene, ElementType::Aperture, {0, 0,  0},      "Fourier Plane");
    placeElement(scene, ElementType::Lens,     {0, 0,  f},      "Lens 2 (f=50mm)");
    placeElement(scene, ElementType::Screen,   {0, 0,  f * 2 + 6}, "Image Plane");

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    addBeam(scene, {0, 0, -14}, {0, 0,  -f},         red);
    addBeam(scene, {0, 0,  -f}, {0, 0,   0},         red);
    addBeam(scene, {0, 0,   0}, {0, 0,   f},         red);
    addBeam(scene, {0, 0,   f}, {0, 0,   f * 2 + 4}, red);

    addTitle(scene, "4f Imaging System", {0, 3, -8});
}

static void buildSpectroscopy(Scene* scene) {
    // Laser → collimating Lens → Grating → Screen
    // Grating disperses into multiple wavelengths at angles
    placeElement(scene, ElementType::Laser,   {0, 0, -14}, "Light Source");
    placeElement(scene, ElementType::Lens,    {0, 0,  -6}, "Collimating Lens");
    placeElement(scene, ElementType::Grating, {0, 0,   0}, "Diffraction Grating", rotY(15));
    placeElement(scene, ElementType::Screen,  {8, 0,   6}, "Detection Screen", rotY(45));

    // White input beam
    glm::vec3 white(0.9f, 0.9f, 0.9f);
    addBeam(scene, {0, 0, -12}, {0, 0, -6}, white);
    addBeam(scene, {0, 0,  -6}, {0, 0,  0}, white);

    // Dispersed beams (red, green, blue at different angles)
    addBeam(scene, {0, 0, 0}, {7, 0, 4},  {1.0f, 0.2f, 0.2f}); // Red
    addBeam(scene, {0, 0, 0}, {7, 0, 5},  {0.2f, 1.0f, 0.2f}); // Green
    addBeam(scene, {0, 0, 0}, {7, 0, 6},  {0.3f, 0.3f, 1.0f}); // Blue

    addTitle(scene, "Spectroscopy Setup", {0, 3, -8});
}

static void buildFiberLink(Scene* scene) {
    // Laser → Fiber Coupler (in) → (fiber, represented as beam) → Fiber Coupler (out) → Detector
    placeElement(scene, ElementType::Laser,        {0, 0, -14}, "Laser Diode");
    placeElement(scene, ElementType::Lens,         {0, 0,  -8}, "Coupling Lens");
    placeElement(scene, ElementType::FiberCoupler, {0, 0,  -4}, "Fiber Input");
    placeElement(scene, ElementType::FiberCoupler, {0, 0,   8}, "Fiber Output");
    placeElement(scene, ElementType::Detector,     {0, 0,  14}, "Photodetector");

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    glm::vec3 orange(1.0f, 0.6f, 0.1f);
    addBeam(scene, {0, 0, -12}, {0, 0, -8}, red);     // Laser → Lens
    addBeam(scene, {0, 0,  -8}, {0, 0, -4}, red);     // Lens → Coupler in
    addBeam(scene, {0, 0,  -4}, {0, 0,  8}, orange, 3.0f); // Fiber (thicker, orange)
    addBeam(scene, {0, 0,   8}, {0, 0, 12}, red);     // Coupler out → Detector

    addTitle(scene, "Fiber Optic Link", {0, 3, -6});
}

static void buildPolarimeter(Scene* scene) {
    // Laser → Polarizer → Sample area → Analyzer → Detector
    placeElement(scene, ElementType::Laser,    {0, 0, -14}, "Laser");
    placeElement(scene, ElementType::Filter,   {0, 0,  -6}, "Polarizer");
    placeElement(scene, ElementType::Aperture, {0, 0,   0}, "Sample");
    placeElement(scene, ElementType::Filter,   {0, 0,   6}, "Analyzer");
    placeElement(scene, ElementType::Detector, {0, 0,  14}, "Detector");

    glm::vec3 red(1.0f, 0.2f, 0.2f);
    addBeam(scene, {0, 0, -12}, {0, 0, -6},  red);
    addBeam(scene, {0, 0,  -6}, {0, 0,  0},  red);
    addBeam(scene, {0, 0,   0}, {0, 0,  6},  red);
    addBeam(scene, {0, 0,   6}, {0, 0, 12},  red);

    addTitle(scene, "Polarimeter", {0, 3, -6});
}

// ── Public API ─────────────────────────────────────────────────────

const std::vector<TemplateInfo>& getTemplateList() {
    static const std::vector<TemplateInfo> templates = {
        {"michelson",     "Michelson Interferometer",     "Interferometers", "Two-arm interferometer with beam splitter"},
        {"mach_zehnder",  "Mach-Zehnder Interferometer",  "Interferometers", "Two-path interferometer with two beam splitters"},
        {"fabry_perot",   "Fabry-Perot Cavity",           "Interferometers", "Inline optical resonator with two mirrors"},
        {"beam_expander", "Beam Expander (3x)",           "Imaging",         "Galilean telescope for beam expansion"},
        {"4f_system",     "4f Imaging System",            "Imaging",         "Fourier optics relay with spatial filter"},
        {"spectroscopy",  "Spectroscopy Setup",           "Spectroscopy",    "Grating-based wavelength dispersion"},
        {"fiber_link",    "Fiber Optic Link",             "Fiber",           "Fiber-coupled laser to detector"},
        {"polarimeter",   "Polarimeter",                  "Polarimetry",     "Polarization measurement setup"},
    };
    return templates;
}

void loadTemplate(const std::string& id, Scene* scene) {
    if (!scene) return;
    scene->clear();

    if (id == "michelson")          buildMichelson(scene);
    else if (id == "mach_zehnder")  buildMachZehnder(scene);
    else if (id == "fabry_perot")   buildFabryPerot(scene);
    else if (id == "beam_expander") buildBeamExpander(scene);
    else if (id == "4f_system")     build4fSystem(scene);
    else if (id == "spectroscopy")  buildSpectroscopy(scene);
    else if (id == "fiber_link")    buildFiberLink(scene);
    else if (id == "polarimeter")   buildPolarimeter(scene);
}

} // namespace opticsketch
