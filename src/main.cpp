#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cmath>
#include <cfloat>
#include <cstdlib>
#include <string>
#include <glm/gtc/quaternion.hpp>
#include <tinyfiledialogs.h>
#include <filesystem>
#include "ui/theme.h"
#include "ui/library_panel.h"
#include "ui/toolbox_panel.h"
#include "ui/outliner_panel.h"
#include "ui/properties_panel.h"
#include "render/viewport.h"
#include "render/raycast.h"
#include "render/gizmo.h"
#include "render/beam.h"
#include "render/mesh_loader.h"
#include "scene/scene.h"
#include "project/project.h"
#include "elements/basic_elements.h"
#include "undo/undo.h"

// Ensure path ends with .optsk for save (so Open can find the file)
static std::string ensureOptskExtension(const std::string& path) {
    if (path.size() >= 6 && path.compare(path.size() - 6, 6, ".optsk") == 0)
        return path;
    return path + ".optsk";
}

// Ensure path ends with .png for export
static std::string ensurePngExtension(const std::string& path) {
    if (path.size() >= 4 && path.compare(path.size() - 4, 4, ".png") == 0)
        return path;
    return path + ".png";
}

// Trim leading/trailing whitespace from path (tinyfd may return trailing newline etc.)
static std::string trimPath(const char* path) {
    if (!path) return std::string();
    std::string s(path);
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return std::string();
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end == std::string::npos ? end : end - start + 1);
}

// Input state
struct InputState {
    bool leftMouseDown = false;
    bool leftMouseJustPressed = false;  // true only on the frame LMB transitions from up to down
    bool middleMouseDown = false;
    bool rightMouseDown = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;
    bool isDraggingCamera = false;  // Track if we're currently dragging camera
    double dragStartX = 0.0;  // Mouse position when drag started
    double dragStartY = 0.0;
    bool wasDragging = false;  // Track if we were dragging in previous frame
    bool spacePressed = false;
    bool altPressed = false;
    bool ctrlPressed = false;
};

// Manipulator drag state (Maya-style move/rotate/scale). Axis locked until left mouse release.
// All in world space: objects and gizmo use world position; move axes are world X/Y/Z.
struct ManipulatorDragState {
    bool active = false;
    int handle = -1;                  // 0=X, 1=Y, 2=Z (world axes)
    glm::vec3 initialGizmoCenter;     // bbox center at drag start (world)
    glm::vec3 initialPosition;
    glm::quat initialRotation;
    glm::vec3 initialScale;
    float initialSignedDist = 0.0f;
    float initialAngle = 0.0f;
    glm::vec3 movePlaneNormal;
    // Move: viewport pos at drag start so we use total movement since start (avoids any double application)
    float initialViewportX = 0.0f;
    float initialViewportY = 0.0f;
    float lastViewportX = 0.0f;
    float lastViewportY = 0.0f;
    int lastApplyFrame = -1;
};

// Application state
struct AppState {
    InputState input;
    opticsketch::Viewport* viewport = nullptr;
};

// Scroll callback - scroll wheel for zoom (even without CTRL)
void scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    AppState* app = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!app || !app->viewport) return;

    if (!ImGui::GetIO().WantCaptureMouse) {
        app->viewport->getCamera().zoom(static_cast<float>(yoffset));
    }
}

int main() {
    // GLFW init
    if (!glfwInit()) {
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #ifdef PLATFORM_LINUX
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    #endif
    
    GLFWwindow* window = glfwCreateWindow(1600, 900, "OpticSketch - Untitled", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);  // VSync
    
    // Load OpenGL functions
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    
    // Create viewport
    opticsketch::Viewport viewport;
    viewport.init(800, 600);
    
    // Create scene
    opticsketch::Scene scene;
    std::string projectPath;  // current .optsk path; empty = untitled

    // Undo/Redo stack
    opticsketch::UndoStack undoStack;

    // Clipboard for copy/paste
    std::unique_ptr<opticsketch::Element> clipboardElement;
    std::unique_ptr<opticsketch::Beam> clipboardBeam;

    // Create library panel
    opticsketch::LibraryPanel libraryPanel;
    
    // Create toolbox panel
    opticsketch::ToolboxPanel toolboxPanel;
    opticsketch::OutlinerPanel outlinerPanel;
    opticsketch::PropertiesPanel propertiesPanel;
    
    // Application state
    AppState app;
    app.viewport = &viewport;
    glfwSetWindowUserPointer(window, &app);

    // Initialize mouse position
    double initMouseX, initMouseY;
    glfwGetCursorPos(window, &initMouseX, &initMouseY);
    app.input.lastMouseX = initMouseX;
    app.input.lastMouseY = initMouseY;

    // Register scroll callback BEFORE ImGui — ImGui chains to it
    glfwSetScrollCallback(window, scrollCallback);

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;    // Enable docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Multi-viewport (optional)

    // Setup Platform/Renderer backends — installs ImGui callbacks that chain to ours
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    // *** APPLY MOONLIGHT THEME ***
    opticsketch::SetupMoonlightTheme();
    opticsketch::SetupFonts();
    
    // Improve font rendering quality
    io.FontGlobalScale = 1.0f; // Ensure no scaling issues
    
    // Enable better text rendering
    ImGuiStyle& style = ImGui::GetStyle();
    style.AntiAliasedLines = true;
    style.AntiAliasedFill = true;
    style.AntiAliasedLinesUseTex = true; // Use texture-based anti-aliasing for lines
    
    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Poll mouse buttons — avoids callback ordering issues with ImGui
        {
            bool prevLeft = app.input.leftMouseDown;
            bool prevMiddle = app.input.middleMouseDown;
            bool prevRight = app.input.rightMouseDown;

            app.input.leftMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            app.input.middleMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
            app.input.rightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

            // Detect press edge (was up, now down)
            app.input.leftMouseJustPressed = app.input.leftMouseDown && !prevLeft;

            if (app.input.leftMouseJustPressed) {
                app.input.wasDragging = false;
            }

            // Stop camera drag when all buttons released
            bool anyDown = app.input.leftMouseDown || app.input.middleMouseDown || app.input.rightMouseDown;
            bool wasAnyDown = prevLeft || prevMiddle || prevRight;
            if (wasAnyDown && !anyDown) {
                app.input.isDraggingCamera = false;
                app.input.wasDragging = false;
            }
        }

        // Update input state
        app.input.spacePressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        app.input.altPressed = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;
        app.input.ctrlPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        
        // Handle keyboard shortcuts (only when not typing in ImGui)
        if (!ImGui::GetIO().WantTextInput) {
            bool ctrl = app.input.ctrlPressed;
            bool shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
            
            // Tool shortcuts (Q, W, E, R, B) - Maya style
            static bool qPressed = false, wPressed = false, ePressed = false, rPressed = false, bPressed = false;
            bool qNow = glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS;
            bool wNow = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            bool eNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
            bool rNow = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
            bool bNow = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
            
            if (qNow && !qPressed) {
                toolboxPanel.setTool(opticsketch::ToolMode::Select);
            }
            if (wNow && !wPressed) {
                toolboxPanel.setTool(opticsketch::ToolMode::Move);
            }
            if (eNow && !ePressed) {
                toolboxPanel.setTool(opticsketch::ToolMode::Rotate);
            }
            if (rNow && !rPressed) {
                toolboxPanel.setTool(opticsketch::ToolMode::Scale);
            }
            if (bNow && !bPressed) {
                toolboxPanel.setTool(opticsketch::ToolMode::DrawBeam);
            }
            
            qPressed = qNow;
            wPressed = wNow;
            ePressed = eNow;
            rPressed = rNow;
            bPressed = bNow;
            
            // File shortcuts
            if (ctrl && glfwGetKey(window, GLFW_KEY_N) == GLFW_PRESS) {
                scene.clear();
                undoStack.clear();
                projectPath.clear();
                glfwSetWindowTitle(window, "OpticSketch - Untitled");
            }
            if (ctrl && shift && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                const char* filters[] = { "*.optsk" };
                const char* path = tinyfd_saveFileDialog("Save OpticSketch Project As", projectPath.empty() ? "untitled.optsk" : projectPath.c_str(), 1, filters, "OpticSketch Project (*.optsk)");
                if (path) {
                    std::string savePath = ensureOptskExtension(path);
                    if (opticsketch::saveProject(savePath, &scene)) {
                        projectPath = savePath;
                        glfwSetWindowTitle(window, ("OpticSketch - " + projectPath.substr(projectPath.find_last_of("/\\") + 1)).c_str());
                    }
                }
            } else if (ctrl && glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                if (projectPath.empty()) {
                    const char* filters[] = { "*.optsk" };
                    const char* path = tinyfd_saveFileDialog("Save OpticSketch Project", "untitled.optsk", 1, filters, "OpticSketch Project (*.optsk)");
                    if (path) {
                        std::string savePath = ensureOptskExtension(path);
                        if (opticsketch::saveProject(savePath, &scene)) {
                            projectPath = savePath;
                            glfwSetWindowTitle(window, ("OpticSketch - " + projectPath.substr(projectPath.find_last_of("/\\") + 1)).c_str());
                        }
                    }
                } else {
                    opticsketch::saveProject(projectPath, &scene);
                }
            }
            if (ctrl && glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
                const char* filters[] = { "*.optsk" };
                const char* path = tinyfd_openFileDialog("Open OpticSketch Project", projectPath.empty() ? nullptr : projectPath.c_str(), 1, filters, "OpticSketch Project (*.optsk)", 0);
                if (path) {
                    std::string openPath = trimPath(path);
                    if (!openPath.empty() && opticsketch::loadProject(openPath, &scene)) {
                        undoStack.clear();
                        projectPath = openPath;
                        glfwSetWindowTitle(window, ("OpticSketch - " + projectPath.substr(projectPath.find_last_of("/\\") + 1)).c_str());
                    } else if (!openPath.empty())
                        tinyfd_messageBox("Open failed", "Could not open project file or file format is invalid.", "ok", "error", 1);
                }
            }
            if (ctrl && glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
                const char* filters[] = { "*.png" };
                const char* path = tinyfd_saveFileDialog("Export PNG", "viewport.png", 1, filters, "PNG image (*.png)");
                if (path) {
                    std::string p = ensurePngExtension(trimPath(path));
                    if (!p.empty()) {
                        if (viewport.exportToPng(p, &scene))
                            tinyfd_messageBox("Export PNG", "Image saved successfully.", "ok", "info", 1);
                        else
                            tinyfd_messageBox("Export failed", "Could not save PNG file.", "ok", "error", 1);
                    }
                }
            }
            
            // Edit shortcuts: Delete/Backspace remove selected element (once per key press, no repeat)
            static bool delPressed = false, backPressed = false;
            bool delNow = glfwGetKey(window, GLFW_KEY_DELETE) == GLFW_PRESS;
            bool backNow = glfwGetKey(window, GLFW_KEY_BACKSPACE) == GLFW_PRESS;
            if ((delNow && !delPressed) || (backNow && !backPressed)) {
                if (scene.getSelectedElement()) {
                    undoStack.push(std::make_unique<opticsketch::RemoveElementCmd>(*scene.getSelectedElement()));
                    scene.removeElement(scene.getSelectedElement()->id);
                } else if (scene.getSelectedBeam()) {
                    undoStack.push(std::make_unique<opticsketch::RemoveBeamCmd>(*scene.getSelectedBeam()));
                    scene.removeBeam(scene.getSelectedBeam()->id);
                }
            }
            delPressed = delNow;
            backPressed = backNow;
            
            // View shortcuts
            if (glfwGetKey(window, GLFW_KEY_HOME) == GLFW_PRESS) {
                viewport.getCamera().resetView();
            }

            // F = Frame Selected, A = Frame All
            static bool fPressed = false, aPressed = false;
            bool fNow = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
            bool aNow = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
            if (fNow && !fPressed) {
                auto* selElem = scene.getSelectedElement();
                auto* selBeam = scene.getSelectedBeam();
                if (selElem) {
                    glm::vec3 bMin, bMax;
                    selElem->getWorldBounds(bMin, bMax);
                    glm::vec3 center = (bMin + bMax) * 0.5f;
                    float radius = glm::length(bMax - bMin) * 0.5f;
                    viewport.getCamera().frameOn(center, radius);
                } else if (selBeam) {
                    glm::vec3 center = (selBeam->start + selBeam->end) * 0.5f;
                    float radius = glm::length(selBeam->end - selBeam->start) * 0.5f;
                    viewport.getCamera().frameOn(center, radius);
                }
            }
            if (aNow && !aPressed) {
                glm::vec3 sceneMin(FLT_MAX), sceneMax(-FLT_MAX);
                bool hasObjects = false;
                for (const auto& elem : scene.getElements()) {
                    if (!elem->visible) continue;
                    glm::vec3 wMin, wMax;
                    elem->getWorldBounds(wMin, wMax);
                    sceneMin = glm::min(sceneMin, wMin);
                    sceneMax = glm::max(sceneMax, wMax);
                    hasObjects = true;
                }
                for (const auto& beam : scene.getBeams()) {
                    if (!beam->visible) continue;
                    sceneMin = glm::min(sceneMin, glm::min(beam->start, beam->end));
                    sceneMax = glm::max(sceneMax, glm::max(beam->start, beam->end));
                    hasObjects = true;
                }
                if (hasObjects) {
                    glm::vec3 center = (sceneMin + sceneMax) * 0.5f;
                    float radius = glm::length(sceneMax - sceneMin) * 0.5f;
                    viewport.getCamera().frameOn(center, radius);
                } else {
                    viewport.getCamera().resetView();
                }
            }
            fPressed = fNow;
            aPressed = aNow;

            // Undo/Redo: Ctrl+Z / Ctrl+Y
            static bool zPressed = false, yPressed = false;
            bool zNow = glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS;
            bool yNow = glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS;
            if (ctrl && zNow && !zPressed) {
                undoStack.undo(scene);
            }
            if (ctrl && yNow && !yPressed) {
                undoStack.redo(scene);
            }
            zPressed = zNow;
            yPressed = yNow;

            // Copy/Cut/Paste: Ctrl+C / Ctrl+X / Ctrl+V
            static bool cPressed = false, xPressed = false, vPressed = false;
            bool cNow = glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS;
            bool xNow = glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS;
            bool vNow = glfwGetKey(window, GLFW_KEY_V) == GLFW_PRESS;
            if (ctrl && cNow && !cPressed) {
                // Copy selected element or beam
                clipboardElement.reset();
                clipboardBeam.reset();
                if (scene.getSelectedElement()) {
                    clipboardElement = scene.getSelectedElement()->clone();
                } else if (scene.getSelectedBeam()) {
                    clipboardBeam = scene.getSelectedBeam()->clone();
                }
            }
            if (ctrl && xNow && !xPressed) {
                // Cut = copy + delete
                clipboardElement.reset();
                clipboardBeam.reset();
                if (scene.getSelectedElement()) {
                    auto* sel = scene.getSelectedElement();
                    clipboardElement = sel->clone();
                    undoStack.push(std::make_unique<opticsketch::RemoveElementCmd>(*sel));
                    scene.removeElement(sel->id);
                } else if (scene.getSelectedBeam()) {
                    auto* sel = scene.getSelectedBeam();
                    clipboardBeam = sel->clone();
                    undoStack.push(std::make_unique<opticsketch::RemoveBeamCmd>(*sel));
                    scene.removeBeam(sel->id);
                }
            }
            if (ctrl && vNow && !vPressed) {
                // Paste from clipboard
                if (clipboardElement) {
                    auto pasted = clipboardElement->clone();
                    pasted->transform.position += glm::vec3(1.0f, 0.0f, 0.0f); // offset
                    std::string pastedId = pasted->id;
                    scene.addElement(std::move(pasted));
                    // ID may have been adjusted by ensureUniqueId — find by scanning last element
                    auto* added = scene.getElements().back().get();
                    undoStack.push(std::make_unique<opticsketch::AddElementCmd>(*added));
                    scene.selectElement(added->id);
                } else if (clipboardBeam) {
                    auto pasted = clipboardBeam->clone();
                    pasted->start += glm::vec3(1.0f, 0.0f, 0.0f);
                    pasted->end += glm::vec3(1.0f, 0.0f, 0.0f);
                    scene.addBeam(std::move(pasted));
                    auto* added = scene.getBeams().back().get();
                    undoStack.push(std::make_unique<opticsketch::AddBeamCmd>(*added));
                    scene.selectBeam(added->id);
                }
            }
            cPressed = cNow;
            xPressed = xNow;
            vPressed = vNow;
        }
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        // Mouse position will be updated after viewport rendering
        
        // About dialog state (declared outside menu bar scope)
        static bool showAboutDialog = false;
        static bool aboutDialogOpen = false;
        static bool viewportWindowVisible = true;

        // Menu bar
        if (ImGui::BeginMainMenuBar()) {
            // File menu
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                    scene.clear();
                    projectPath.clear();
                    glfwSetWindowTitle(window, "OpticSketch - Untitled");
                }
                if (ImGui::MenuItem("Open Project...", "Ctrl+O")) {
                    const char* filters[] = { "*.optsk" };
                    const char* path = tinyfd_openFileDialog("Open OpticSketch Project", projectPath.empty() ? nullptr : projectPath.c_str(), 1, filters, "OpticSketch Project (*.optsk)", 0);
                    if (path) {
                        std::string openPath = trimPath(path);
                        if (!openPath.empty() && opticsketch::loadProject(openPath, &scene)) {
                            projectPath = openPath;
                            glfwSetWindowTitle(window, ("OpticSketch - " + projectPath.substr(projectPath.find_last_of("/\\") + 1)).c_str());
                        } else if (!openPath.empty())
                            tinyfd_messageBox("Open failed", "Could not open project file or file format is invalid.", "ok", "error", 1);
                    }
                }
                if (ImGui::MenuItem("Save Project", "Ctrl+S")) {
                    if (projectPath.empty()) {
                        const char* filters[] = { "*.optsk" };
                        const char* path = tinyfd_saveFileDialog("Save OpticSketch Project", "untitled.optsk", 1, filters, "OpticSketch Project (*.optsk)");
                        if (path) {
                            std::string savePath = ensureOptskExtension(path);
                            if (opticsketch::saveProject(savePath, &scene)) {
                                projectPath = savePath;
                                glfwSetWindowTitle(window, ("OpticSketch - " + projectPath.substr(projectPath.find_last_of("/\\") + 1)).c_str());
                            }
                        }
                    } else {
                        opticsketch::saveProject(projectPath, &scene);
                    }
                }
                if (ImGui::MenuItem("Save Project As...", "Ctrl+Shift+S")) {
                    const char* filters[] = { "*.optsk" };
                    const char* path = tinyfd_saveFileDialog("Save OpticSketch Project As", projectPath.empty() ? "untitled.optsk" : projectPath.c_str(), 1, filters, "OpticSketch Project (*.optsk)");
                    if (path) {
                        std::string savePath = ensureOptskExtension(path);
                        if (opticsketch::saveProject(savePath, &scene)) {
                            projectPath = savePath;
                            glfwSetWindowTitle(window, ("OpticSketch - " + projectPath.substr(projectPath.find_last_of("/\\") + 1)).c_str());
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export PNG...", "Ctrl+E")) {
                    const char* filters[] = { "*.png" };
                    const char* path = tinyfd_saveFileDialog("Export PNG", "viewport.png", 1, filters, "PNG image (*.png)");
                    if (path) {
                        std::string p = ensurePngExtension(trimPath(path));
                        if (!p.empty()) {
                            if (viewport.exportToPng(p, &scene))
                                tinyfd_messageBox("Export PNG", "Image saved successfully.", "ok", "info", 1);
                            else
                                tinyfd_messageBox("Export failed", "Could not save PNG file.", "ok", "error", 1);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Import OBJ...")) {
                    const char* filters[] = { "*.obj" };
                    const char* path = tinyfd_openFileDialog("Import OBJ Mesh", nullptr, 1, filters, "Wavefront OBJ (*.obj)", 0);
                    if (path) {
                        std::string objPath = trimPath(path);
                        if (!objPath.empty()) {
                            // Copy to assets/meshes/ folder
                            namespace fs = std::filesystem;
                            fs::path src(objPath);
                            fs::path meshDir = fs::path(".") / "assets" / "meshes";
                            fs::create_directories(meshDir);
                            fs::path dst = meshDir / src.filename();
                            if (src != dst) {
                                std::error_code ec;
                                fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
                                if (!ec) objPath = dst.string();
                            }
                            // Derive display name from filename
                            std::string name = src.stem().string();
                            libraryPanel.addImportedItem(name, objPath);
                        }
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) {
                    glfwSetWindowShouldClose(window, true);
                }
                ImGui::EndMenu();
            }
            
            // Edit menu
            bool hasSelection = scene.getSelectedElement() != nullptr || scene.getSelectedBeam() != nullptr;
            bool hasClipboard = clipboardElement != nullptr || clipboardBeam != nullptr;
            if (ImGui::BeginMenu("Edit")) {
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, undoStack.canUndo())) {
                    undoStack.undo(scene);
                }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, undoStack.canRedo())) {
                    undoStack.redo(scene);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasSelection)) {
                    clipboardElement.reset();
                    clipboardBeam.reset();
                    if (scene.getSelectedElement()) {
                        auto* sel = scene.getSelectedElement();
                        clipboardElement = sel->clone();
                        undoStack.push(std::make_unique<opticsketch::RemoveElementCmd>(*sel));
                        scene.removeElement(sel->id);
                    } else if (scene.getSelectedBeam()) {
                        auto* sel = scene.getSelectedBeam();
                        clipboardBeam = sel->clone();
                        undoStack.push(std::make_unique<opticsketch::RemoveBeamCmd>(*sel));
                        scene.removeBeam(sel->id);
                    }
                }
                if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection)) {
                    clipboardElement.reset();
                    clipboardBeam.reset();
                    if (scene.getSelectedElement()) {
                        clipboardElement = scene.getSelectedElement()->clone();
                    } else if (scene.getSelectedBeam()) {
                        clipboardBeam = scene.getSelectedBeam()->clone();
                    }
                }
                if (ImGui::MenuItem("Paste", "Ctrl+V", false, hasClipboard)) {
                    if (clipboardElement) {
                        auto pasted = clipboardElement->clone();
                        pasted->transform.position += glm::vec3(1.0f, 0.0f, 0.0f);
                        scene.addElement(std::move(pasted));
                        auto* added = scene.getElements().back().get();
                        undoStack.push(std::make_unique<opticsketch::AddElementCmd>(*added));
                        scene.selectElement(added->id);
                    } else if (clipboardBeam) {
                        auto pasted = clipboardBeam->clone();
                        pasted->start += glm::vec3(1.0f, 0.0f, 0.0f);
                        pasted->end += glm::vec3(1.0f, 0.0f, 0.0f);
                        scene.addBeam(std::move(pasted));
                        auto* added = scene.getBeams().back().get();
                        undoStack.push(std::make_unique<opticsketch::AddBeamCmd>(*added));
                        scene.selectBeam(added->id);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete", "Del", false, hasSelection)) {
                    if (scene.getSelectedElement()) {
                        undoStack.push(std::make_unique<opticsketch::RemoveElementCmd>(*scene.getSelectedElement()));
                        scene.removeElement(scene.getSelectedElement()->id);
                    } else if (scene.getSelectedBeam()) {
                        undoStack.push(std::make_unique<opticsketch::RemoveBeamCmd>(*scene.getSelectedBeam()));
                        scene.removeBeam(scene.getSelectedBeam()->id);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Select All", "Ctrl+A")) {
                    // TODO: Select all
                }
                ImGui::EndMenu();
            }
            
            // View menu
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("2D Top-Down", nullptr, viewport.getCamera().mode == opticsketch::CameraMode::TopDown2D)) {
                    viewport.getCamera().setMode(opticsketch::CameraMode::TopDown2D);
                }
                if (ImGui::MenuItem("3D Orthographic", nullptr, viewport.getCamera().mode == opticsketch::CameraMode::Orthographic3D)) {
                    viewport.getCamera().setMode(opticsketch::CameraMode::Orthographic3D);
                }
                if (ImGui::MenuItem("3D Perspective", nullptr, viewport.getCamera().mode == opticsketch::CameraMode::Perspective3D)) {
                    viewport.getCamera().setMode(opticsketch::CameraMode::Perspective3D);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Reset View", "Home")) {
                    viewport.getCamera().resetView();
                }
                ImGui::EndMenu();
            }
            
            // Windows menu
            if (ImGui::BeginMenu("Windows")) {
                if (ImGui::MenuItem("Viewport", nullptr, viewportWindowVisible)) {
                    viewportWindowVisible = !viewportWindowVisible;
                }
                if (ImGui::MenuItem("Outliner", nullptr, outlinerPanel.isVisible())) {
                    outlinerPanel.setVisible(!outlinerPanel.isVisible());
                }
                if (ImGui::MenuItem("Library", nullptr, libraryPanel.isVisible())) {
                    libraryPanel.setVisible(!libraryPanel.isVisible());
                }
                if (ImGui::MenuItem("Properties", nullptr, propertiesPanel.isVisible())) {
                    propertiesPanel.setVisible(!propertiesPanel.isVisible());
                }
                if (ImGui::MenuItem("Toolbox", nullptr, toolboxPanel.isVisible())) {
                    toolboxPanel.setVisible(!toolboxPanel.isVisible());
                }
                ImGui::EndMenu();
            }
            
            // Help menu
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About OpticSketch...")) {
                    showAboutDialog = true;
                }
                ImGui::EndMenu();
            }
            
            ImGui::EndMainMenuBar();
        }
        
        // About dialog
        if (showAboutDialog) {
            ImGui::OpenPopup("About OpticSketch");
            showAboutDialog = false;
            aboutDialogOpen = true;
        }
        
        if (ImGui::BeginPopupModal("About OpticSketch", &aboutDialogOpen, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("OpticSketch");
            ImGui::Separator();
            ImGui::Text("Version 0.1.0");
            ImGui::Text("A lightweight desktop application for creating");
            ImGui::Text("publication-quality optical bench diagrams.");
            ImGui::Spacing();
            ImGui::Text("Built with:");
            ImGui::BulletText("C++17");
            ImGui::BulletText("ImGui (Moonlight theme)");
            ImGui::BulletText("OpenGL 3.3");
            ImGui::BulletText("GLFW");
            ImGui::BulletText("GLM");
            ImGui::Spacing();
            ImGui::Text("License: MIT");
            ImGui::Spacing();
            ImGui::Text("");
            ImGui::Text("Copyright 2025:");
            ImGui::Text("Jacopo Bertolotti & ");
            ImGui::SameLine(0, 0);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.6f, 1.0f, 1.0f));
            if (ImGui::Selectable("Damn! LTD", false, 0, ImGui::CalcTextSize("Damn! LTD"))) {
                #ifdef PLATFORM_LINUX
                    std::system("xdg-open http://www.damnltd.com");
                #elif defined(PLATFORM_WINDOWS)
                    std::system("start http://www.damnltd.com");
                #elif defined(__APPLE__)
                    std::system("open http://www.damnltd.com");
                #else
                    std::system("xdg-open http://www.damnltd.com");
                #endif
            }
            ImGui::PopStyleColor();

            ImGui::Spacing();
            float buttonWidth = 120.0f;
            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - buttonWidth) * 0.5f);
            if (ImGui::Button("OK", ImVec2(buttonWidth, 0))) {
                ImGui::CloseCurrentPopup();
                aboutDialogOpen = false;
            }
            ImGui::EndPopup();
        }

        // Full-window dockspace
        ImGui::DockSpaceOverViewport();
        
        // Status bar at bottom (simpler approach - let docking handle it)
        ImGui::Begin("Status", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
        
        ImGui::Text("Ready");
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        ImGui::Text("Elements: %zu", scene.getElements().size());
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        const char* modeStr = "";
        switch (viewport.getCamera().mode) {
            case opticsketch::CameraMode::TopDown2D: modeStr = "2D Top-Down"; break;
            case opticsketch::CameraMode::Orthographic3D: modeStr = "3D Orthographic"; break;
            case opticsketch::CameraMode::Perspective3D: modeStr = "3D Perspective"; break;
        }
        ImGui::Text("Camera: %s", modeStr);
        ImGui::End();
        
        // Library Panel
        libraryPanel.render();
        
        // Toolbox Panel
        toolboxPanel.render();
        
        // Outliner (scene structure)
        outlinerPanel.render(&scene);
        
        // Properties (selected element)
        propertiesPanel.render(&scene);
        
        // Viewport window
        if (ImGui::Begin("3D Viewport", &viewportWindowVisible)) {
        
        // Viewport toolbar
        ImGui::BeginGroup();
        
        // Camera mode selector
        const char* modes[] = {"2D Top", "3D Ortho", "3D Persp"};
        int currentMode = static_cast<int>(viewport.getCamera().mode);
        if (ImGui::Combo("Mode", &currentMode, modes, 3)) {
            viewport.getCamera().setMode(static_cast<opticsketch::CameraMode>(currentMode));
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Home")) {
            viewport.getCamera().resetView();
        }
        
        ImGui::SameLine();
        ImGui::Text("|");
        ImGui::SameLine();
        ImGui::Text("Elements: %zu", scene.getElements().size());
        
        ImGui::EndGroup();
        
        ImGui::Separator();
        
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();
        int vpWidth = static_cast<int>(viewportSize.x);
        int vpHeight = static_cast<int>(viewportSize.y);
        
        if (vpWidth > 0 && vpHeight > 0) {
            static int lastGizmoHoveredHandle = -1;  // for axis hover highlight (updated after we have viewport mouse pos)
            if (viewport.getWidth() != vpWidth || viewport.getHeight() != vpHeight) {
                viewport.resize(vpWidth, vpHeight);
            }
            
            // Draw viewport image first so we get current-frame rect; then input + drag use same rect for realtime follow
            ImGui::Image(reinterpret_cast<void*>(static_cast<intptr_t>(viewport.getTextureId())),
                        viewportSize,
                        ImVec2(0, 1), ImVec2(1, 0)); // Flip Y
            ImVec2 imageMin = ImGui::GetItemRectMin();
            bool isViewportHovered = ImGui::IsItemHovered();
            static ImVec2 lastImageMin(0.0f, 0.0f);
            lastImageMin = imageMin;
            ImGuiIO& io = ImGui::GetIO();
            float viewportX = io.MousePos.x - imageMin.x;
            float viewportY = io.MousePos.y - imageMin.y;
            double mouseX = static_cast<double>(io.MousePos.x);
            double mouseY = static_cast<double>(io.MousePos.y);
            bool ctrlPressed = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                              glfwGetKey(window, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
            bool inViewport = (viewportX >= 0 && viewportX < viewportSize.x && viewportY >= 0 && viewportY < viewportSize.y);
            static ManipulatorDragState manipDrag;
            opticsketch::Element* selectedElem = scene.getSelectedElement();
            opticsketch::ToolMode currentTool = toolboxPanel.getCurrentTool();
            if (selectedElem && currentTool != opticsketch::ToolMode::Select && inViewport) {
                opticsketch::GizmoType gType = (currentTool == opticsketch::ToolMode::Move) ? opticsketch::GizmoType::Move :
                    (currentTool == opticsketch::ToolMode::Rotate) ? opticsketch::GizmoType::Rotate : opticsketch::GizmoType::Scale;
                lastGizmoHoveredHandle = viewport.getGizmoHoveredHandle(selectedElem, gType, viewportX, viewportY);
            } else {
                lastGizmoHoveredHandle = -1;
            }
            if (!app.input.leftMouseDown && manipDrag.active) {
                // Drag just ended — push transform undo command
                auto* selE = scene.getSelectedElement();
                if (selE) {
                    opticsketch::Transform newT = selE->transform;
                    opticsketch::Transform oldT;
                    oldT.position = manipDrag.initialPosition;
                    oldT.rotation = manipDrag.initialRotation;
                    oldT.scale = manipDrag.initialScale;
                    // Only push if transform actually changed
                    if (oldT.position != newT.position || oldT.rotation != newT.rotation || oldT.scale != newT.scale) {
                        undoStack.push(std::make_unique<opticsketch::TransformElementCmd>(selE->id, oldT, newT));
                    }
                }
                manipDrag.active = false;
            }
            if (!app.input.leftMouseDown) manipDrag.active = false;
            static bool selectionBoxActive = false;
            static float selectionBoxStartX = 0.0f, selectionBoxStartY = 0.0f;
            bool clickStartedInViewport = inViewport && app.input.leftMouseJustPressed
                                          && !ImGui::GetDragDropPayload();
            
            // Beam drawing mode: click to place beam points
            static glm::vec3 beamStartPoint;
            static bool beamDrawingActive = false;
            static bool beamStartPlaced = false;
            static glm::vec3 beamPreviewEnd;
            
            // Update preview end point when hovering in DrawBeam mode
            if (currentTool == opticsketch::ToolMode::DrawBeam && inViewport && beamStartPlaced) {
                opticsketch::Raycast::Ray ray = opticsketch::Raycast::screenToRay(
                    viewport.getCamera(), viewportX, viewportY, vpWidth, vpHeight);
                float t = -ray.origin.y / ray.direction.y;
                if (t > 0.0f && std::abs(ray.direction.y) > 1e-5f) {
                    beamPreviewEnd = ray.origin + t * ray.direction;
                }
            }
            
            if (currentTool == opticsketch::ToolMode::DrawBeam && clickStartedInViewport && !ctrlPressed && !app.input.isDraggingCamera) {
                opticsketch::Raycast::Ray ray = opticsketch::Raycast::screenToRay(
                    viewport.getCamera(), viewportX, viewportY, vpWidth, vpHeight);
                
                // Intersect with ground plane (Y=0) for beam placement
                float t = -ray.origin.y / ray.direction.y;
                if (t > 0.0f && std::abs(ray.direction.y) > 1e-5f) {
                    glm::vec3 hitPoint = ray.origin + t * ray.direction;
                    
                    if (!beamStartPlaced) {
                        // Place first point
                        beamStartPoint = hitPoint;
                        beamPreviewEnd = hitPoint;
                        beamStartPlaced = true;
                        beamDrawingActive = true;
                    } else {
                        // Place second point and create beam
                        auto beam = std::make_unique<opticsketch::Beam>();
                        beam->start = beamStartPoint;
                        beam->end = hitPoint;
                        beam->color = glm::vec3(1.0f, 0.2f, 0.2f); // Red laser beam
                        std::string beamId = beam->id;
                        scene.addBeam(std::move(beam));
                        undoStack.push(std::make_unique<opticsketch::AddBeamCmd>(*scene.getBeams().back()));
                        scene.selectBeam(beamId);
                        
                        // Reset for next beam
                        beamStartPlaced = false;
                        beamDrawingActive = false;
                    }
                }
            }
            
            // Reset beam drawing if tool changes
            if (currentTool != opticsketch::ToolMode::DrawBeam) {
                beamStartPlaced = false;
                beamDrawingActive = false;
            }
            
            // Store preview beam for rendering (don't add to scene)
            static opticsketch::Beam* previewBeamPtr = nullptr;
            static opticsketch::Beam previewBeam;
            if (currentTool == opticsketch::ToolMode::DrawBeam && beamStartPlaced) {
                previewBeam.start = beamStartPoint;
                previewBeam.end = beamPreviewEnd;
                previewBeam.color = glm::vec3(1.0f, 0.5f, 0.5f); // Lighter red for preview
                previewBeam.width = 1.5f;
                previewBeamPtr = &previewBeam;
            } else {
                previewBeamPtr = nullptr;
            }
            
            if (!ctrlPressed && clickStartedInViewport && !app.input.isDraggingCamera && currentTool != opticsketch::ToolMode::DrawBeam) {
                int hoveredHandle = -1;
                if (selectedElem && currentTool != opticsketch::ToolMode::Select) {
                    opticsketch::GizmoType gType = (currentTool == opticsketch::ToolMode::Move) ? opticsketch::GizmoType::Move :
                        (currentTool == opticsketch::ToolMode::Rotate) ? opticsketch::GizmoType::Rotate : opticsketch::GizmoType::Scale;
                    hoveredHandle = viewport.getGizmoHoveredHandle(selectedElem, gType, viewportX, viewportY);
                }
                if (hoveredHandle >= 0 && !manipDrag.active) {
                    manipDrag.active = true;
                    manipDrag.handle = hoveredHandle;
                    manipDrag.initialGizmoCenter = selectedElem->getWorldBoundsCenter();
                    manipDrag.initialPosition = selectedElem->transform.position;
                    manipDrag.initialRotation = selectedElem->transform.rotation;
                    manipDrag.initialScale = selectedElem->transform.scale;
                    opticsketch::Raycast::Ray startRay = opticsketch::Raycast::screenToRay(
                        viewport.getCamera(), viewportX, viewportY, vpWidth, vpHeight);
                    glm::vec3 axisDir;
                    float axisLenOrRadius;
                    if (currentTool == opticsketch::ToolMode::Move) {
                        opticsketch::Gizmo::getMoveAxis(hoveredHandle, axisDir, axisLenOrRadius);
                        manipDrag.initialViewportX = viewportX;
                        manipDrag.initialViewportY = viewportY;
                        manipDrag.lastViewportX = viewportX;
                        manipDrag.lastViewportY = viewportY;
                    } else if (currentTool == opticsketch::ToolMode::Rotate) {
                        opticsketch::Gizmo::getRotateAxis(hoveredHandle, axisDir, axisLenOrRadius);
                        float t;
                        if (opticsketch::Raycast::intersectPlane(startRay, manipDrag.initialGizmoCenter, axisDir, t)) {
                            glm::vec3 hit = startRay.origin + t * startRay.direction;
                            glm::vec3 toHit = hit - manipDrag.initialGizmoCenter;
                            glm::vec3 viewDir = glm::normalize(viewport.getCamera().position - manipDrag.initialGizmoCenter);
                            glm::vec3 refRight = glm::normalize(glm::cross(axisDir, viewDir));
                            glm::vec3 refUp = glm::cross(axisDir, refRight);
                            manipDrag.initialAngle = std::atan2(glm::dot(toHit, refUp), glm::dot(toHit, refRight));
                        }
                    } else if (currentTool == opticsketch::ToolMode::Scale) {
                        opticsketch::Gizmo::getMoveAxis(hoveredHandle, axisDir, axisLenOrRadius);
                        glm::vec3 viewDir = glm::normalize(viewport.getCamera().position - manipDrag.initialGizmoCenter);
                        glm::vec3 crossVec = glm::cross(axisDir, viewDir);
                        float len = glm::length(crossVec);
                        if (len < 1e-5f) {
                            if (std::abs(glm::dot(axisDir, glm::vec3(0, 1, 0))) < 0.9f)
                                manipDrag.movePlaneNormal = glm::normalize(glm::cross(axisDir, glm::vec3(0, 1, 0)));
                            else
                                manipDrag.movePlaneNormal = glm::normalize(glm::cross(axisDir, glm::vec3(1, 0, 0)));
                        } else {
                            manipDrag.movePlaneNormal = crossVec / len;
                        }
                        float t;
                        if (opticsketch::Raycast::intersectPlane(startRay, manipDrag.initialGizmoCenter, manipDrag.movePlaneNormal, t)) {
                            glm::vec3 P = startRay.origin + t * startRay.direction;
                            manipDrag.initialSignedDist = glm::dot(P - manipDrag.initialGizmoCenter, axisDir);
                        }
                    }
                } else {
                    if (currentTool == opticsketch::ToolMode::Select) {
                        // Start selection box in Select tool
                        if (!selectionBoxActive && inViewport) {
                            selectionBoxStartX = viewportX;
                            selectionBoxStartY = viewportY;
                            selectionBoxActive = true;
                        }
                    } else {
                        // In Move/Rotate/Scale mode: click-select another object (gizmo stays with new selection)
                        opticsketch::Raycast::Ray ray = opticsketch::Raycast::screenToRay(
                            viewport.getCamera(), viewportX, viewportY, vpWidth, vpHeight);
                        float closestT = std::numeric_limits<float>::max();
                        opticsketch::Element* closestElement = nullptr;
                        for (const auto& elem : scene.getElements()) {
                            if (!elem->visible) continue;
                            glm::mat4 transform = elem->transform.getMatrix();
                            float t;
                            if (opticsketch::Raycast::intersectElement(ray, elem.get(), transform, t)) {
                                if (t > 0.0f && t < closestT) {
                                    closestT = t;
                                    closestElement = elem.get();
                                }
                            }
                        }
                        // Also test beams (ray-to-segment distance)
                        opticsketch::Beam* closestBeam = nullptr;
                        float beamPickThreshold = 0.3f; // world-space pick radius
                        float bestBeamDist = beamPickThreshold * beamPickThreshold;
                        for (const auto& beam : scene.getBeams()) {
                            if (!beam->visible) continue;
                            float tRay, tSeg;
                            float sqDist = opticsketch::Raycast::rayToSegmentSqDist(ray, beam->start, beam->end, tRay, tSeg);
                            if (sqDist < bestBeamDist && tRay > 0.0f) {
                                bestBeamDist = sqDist;
                                closestBeam = beam.get();
                            }
                        }
                        // Elements take priority over beams
                        if (closestElement) scene.selectElement(closestElement->id);
                        else if (closestBeam) scene.selectBeam(closestBeam->id);
                        else scene.deselectAll();
                    }
                }
            }
            // Cancel rectangle selection when mouse leaves viewport
            if (selectionBoxActive && !inViewport) {
                selectionBoxActive = false;
            }
            if (selectionBoxActive && !app.input.leftMouseDown) {
                const float dragThreshold = 5.0f;
                float dx = viewportX - selectionBoxStartX;
                float dy = viewportY - selectionBoxStartY;
                float dragLen = std::sqrt(dx * dx + dy * dy);
                const opticsketch::Camera& cam = viewport.getCamera();
                glm::mat4 view = cam.getViewMatrix();
                glm::mat4 proj = cam.getProjectionMatrix();
                auto worldToViewport = [&](const glm::vec3& worldPos, float& outVx, float& outVy) -> bool {
                    glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
                    if (clip.w <= 0.0f) return false;
                    float ndcX = clip.x / clip.w;
                    float ndcY = clip.y / clip.w;
                    outVx = (ndcX * 0.5f + 0.5f) * vpWidth;
                    outVy = (1.0f - (ndcY * 0.5f + 0.5f)) * vpHeight;
                    return true;
                };
                if (dragLen > dragThreshold) {
                    float rminX = std::min(selectionBoxStartX, viewportX);
                    float rmaxX = std::max(selectionBoxStartX, viewportX);
                    float rminY = std::min(selectionBoxStartY, viewportY);
                    float rmaxY = std::max(selectionBoxStartY, viewportY);
                    opticsketch::Element* bestElem = nullptr;
                    float bestDepth = std::numeric_limits<float>::max();
                    for (const auto& elem : scene.getElements()) {
                        if (!elem->visible) continue;
                        glm::vec3 wMin, wMax;
                        elem->getWorldBounds(wMin, wMax);
                        float vx, vy;
                        float minVx = std::numeric_limits<float>::max(), maxVx = -std::numeric_limits<float>::max();
                        float minVy = std::numeric_limits<float>::max(), maxVy = -std::numeric_limits<float>::max();
                        bool anyVisible = false;
                        glm::vec3 corners[8] = {
                            glm::vec3(wMin.x, wMin.y, wMin.z), glm::vec3(wMax.x, wMin.y, wMin.z),
                            glm::vec3(wMin.x, wMax.y, wMin.z), glm::vec3(wMax.x, wMax.y, wMin.z),
                            glm::vec3(wMin.x, wMin.y, wMax.z), glm::vec3(wMax.x, wMin.y, wMax.z),
                            glm::vec3(wMin.x, wMax.y, wMax.z), glm::vec3(wMax.x, wMax.y, wMax.z)
                        };
                        for (int i = 0; i < 8; i++) {
                            if (worldToViewport(corners[i], vx, vy)) {
                                anyVisible = true;
                                minVx = std::min(minVx, vx); maxVx = std::max(maxVx, vx);
                                minVy = std::min(minVy, vy); maxVy = std::max(maxVy, vy);
                            }
                        }
                        if (!anyVisible) continue;
                        bool overlaps = (rminX <= maxVx && rmaxX >= minVx && rminY <= maxVy && rmaxY >= minVy);
                        if (!overlaps) continue;
                        glm::vec3 center = elem->getWorldBoundsCenter();
                        float depth = glm::length(center - cam.position);
                        if (depth < bestDepth) {
                            bestDepth = depth;
                            bestElem = elem.get();
                        }
                    }
                    // Also test beams (project both endpoints)
                    opticsketch::Beam* bestBeam = nullptr;
                    float bestBeamDepth = std::numeric_limits<float>::max();
                    for (const auto& beam : scene.getBeams()) {
                        if (!beam->visible) continue;
                        float sv, ev;
                        float svx, svy, evx, evy;
                        bool startVis = worldToViewport(beam->start, svx, svy);
                        bool endVis = worldToViewport(beam->end, evx, evy);
                        if (!startVis && !endVis) continue;
                        float bminVx = std::min(svx, evx), bmaxVx = std::max(svx, evx);
                        float bminVy = std::min(svy, evy), bmaxVy = std::max(svy, evy);
                        bool overlaps = (rminX <= bmaxVx && rmaxX >= bminVx && rminY <= bmaxVy && rmaxY >= bminVy);
                        if (!overlaps) continue;
                        glm::vec3 mid = (beam->start + beam->end) * 0.5f;
                        float depth = glm::length(mid - cam.position);
                        if (depth < bestBeamDepth) {
                            bestBeamDepth = depth;
                            bestBeam = beam.get();
                        }
                    }
                    // Elements take priority
                    if (bestElem) scene.selectElement(bestElem->id);
                    else if (bestBeam) scene.selectBeam(bestBeam->id);
                    else scene.deselectAll();
                } else {
                    opticsketch::Raycast::Ray ray = opticsketch::Raycast::screenToRay(cam, selectionBoxStartX, selectionBoxStartY, vpWidth, vpHeight);
                    float closestT = std::numeric_limits<float>::max();
                    opticsketch::Element* closestElement = nullptr;
                    for (const auto& elem : scene.getElements()) {
                        if (!elem->visible) continue;
                        glm::mat4 transform = elem->transform.getMatrix();
                        float t;
                        if (opticsketch::Raycast::intersectElement(ray, elem.get(), transform, t)) {
                            if (t > 0.0f && t < closestT) {
                                closestT = t;
                                closestElement = elem.get();
                            }
                        }
                    }
                    // Also test beams
                    opticsketch::Beam* closestBeam = nullptr;
                    float beamPickThreshold = 0.3f;
                    float bestBeamDist = beamPickThreshold * beamPickThreshold;
                    for (const auto& beam : scene.getBeams()) {
                        if (!beam->visible) continue;
                        float tRay, tSeg;
                        float sqDist = opticsketch::Raycast::rayToSegmentSqDist(ray, beam->start, beam->end, tRay, tSeg);
                        if (sqDist < bestBeamDist && tRay > 0.0f) {
                            bestBeamDist = sqDist;
                            closestBeam = beam.get();
                        }
                    }
                    if (closestElement) scene.selectElement(closestElement->id);
                    else if (closestBeam) scene.selectBeam(closestBeam->id);
                    else scene.deselectAll();
                }
                selectionBoxActive = false;
            }
            // Apply manipulator drag with current-frame viewport coords (realtime: handle = bbox centroid follows mouse)
            if (manipDrag.active && selectedElem && app.input.leftMouseDown) {
                opticsketch::Raycast::Ray ray = opticsketch::Raycast::screenToRay(
                    viewport.getCamera(), viewportX, viewportY, vpWidth, vpHeight);
                if (currentTool == opticsketch::ToolMode::Move) {
                    // Accumulation from drag start: desired center = initial + total mouse movement along axis (no per-frame delta = no double transform)
                    int frame = ImGui::GetFrameCount();
                    if (frame != manipDrag.lastApplyFrame) {
                        manipDrag.lastApplyFrame = frame;
                        glm::vec3 axisDir;
                        float axisLen;
                        opticsketch::Gizmo::getMoveAxis(manipDrag.handle, axisDir, axisLen);
                        glm::vec3 currentCenter = selectedElem->getWorldBoundsCenter();
                        const opticsketch::Camera& cam = viewport.getCamera();
                        glm::mat4 view = cam.getViewMatrix();
                        glm::mat4 proj = cam.getProjectionMatrix();
                        auto worldToViewport = [&](const glm::vec3& worldPos, float& outVx, float& outVy) -> bool {
                            glm::vec4 clip = proj * view * glm::vec4(worldPos, 1.0f);
                            if (clip.w <= 0.0f) return false;
                            float ndcX = clip.x / clip.w;
                            float ndcY = clip.y / clip.w;
                            outVx = (ndcX * 0.5f + 0.5f) * vpWidth;
                            outVy = (1.0f - (ndcY * 0.5f + 0.5f)) * vpHeight;
                            return true;
                        };
                        float vx0, vy0, vx1, vy1;
                        if (worldToViewport(currentCenter, vx0, vy0) &&
                            worldToViewport(currentCenter + axisDir, vx1, vy1)) {
                            float dx = vx1 - vx0;
                            float dy = vy1 - vy0;
                            float axisScreenLen = std::sqrt(dx * dx + dy * dy);
                            if (axisScreenLen > 1e-5f) {
                                float invLen = 1.0f / axisScreenLen;
                                float axisScreenX = dx * invLen;
                                float axisScreenY = dy * invLen;
                                float totalPixelAlongAxis = (viewportX - manipDrag.initialViewportX) * axisScreenX +
                                                           (viewportY - manipDrag.initialViewportY) * axisScreenY;
                                float worldTotal = totalPixelAlongAxis / axisScreenLen;
                                glm::vec3 newCenter = manipDrag.initialGizmoCenter + axisDir * worldTotal;
                                glm::vec3 localCenter = selectedElem->getLocalBoundsCenter();
                                glm::vec3 offset = glm::vec3(manipDrag.initialRotation * glm::vec4(manipDrag.initialScale * localCenter, 0.0f));
                                selectedElem->transform.position = newCenter - offset;
                            }
                        }
                        manipDrag.lastViewportX = viewportX;
                        manipDrag.lastViewportY = viewportY;
                    }
                } else if (currentTool == opticsketch::ToolMode::Rotate) {
                    int frame = ImGui::GetFrameCount();
                    if (frame != manipDrag.lastApplyFrame) {
                        manipDrag.lastApplyFrame = frame;
                        glm::vec3 axisDir;
                        float radius;
                        opticsketch::Gizmo::getRotateAxis(manipDrag.handle, axisDir, radius);
                        float t;
                        if (opticsketch::Raycast::intersectPlane(ray, manipDrag.initialGizmoCenter, axisDir, t)) {
                            glm::vec3 hit = ray.origin + t * ray.direction;
                            glm::vec3 toHit = hit - manipDrag.initialGizmoCenter;
                            glm::vec3 viewDir = glm::normalize(viewport.getCamera().position - manipDrag.initialGizmoCenter);
                            glm::vec3 refRight = glm::normalize(glm::cross(axisDir, viewDir));
                            glm::vec3 refUp = glm::cross(axisDir, refRight);
                            float currentAngle = std::atan2(glm::dot(toHit, refUp), glm::dot(toHit, refRight));
                            float deltaAngle = currentAngle - manipDrag.initialAngle;
                            selectedElem->transform.rotation = glm::rotate(manipDrag.initialRotation, deltaAngle, axisDir);
                            glm::vec3 localCenter = selectedElem->getLocalBoundsCenter();
                            glm::vec3 offset = glm::vec3(selectedElem->transform.rotation * glm::vec4(selectedElem->transform.scale * localCenter, 0.0f));
                            selectedElem->transform.position = manipDrag.initialGizmoCenter - offset;
                        }
                    }
                } else if (currentTool == opticsketch::ToolMode::Scale) {
                    int frame = ImGui::GetFrameCount();
                    if (frame != manipDrag.lastApplyFrame) {
                        manipDrag.lastApplyFrame = frame;
                        glm::vec3 axisDir;
                        float axisLen;
                        opticsketch::Gizmo::getMoveAxis(manipDrag.handle, axisDir, axisLen);
                        float t;
                        if (opticsketch::Raycast::intersectPlane(ray, manipDrag.initialGizmoCenter, manipDrag.movePlaneNormal, t)) {
                            glm::vec3 P = ray.origin + t * ray.direction;
                            float signedDist = glm::dot(P - manipDrag.initialGizmoCenter, axisDir);
                            float delta = signedDist - manipDrag.initialSignedDist;
                            glm::vec3 s = manipDrag.initialScale;
                            if (manipDrag.handle == 0) selectedElem->transform.scale.x = s.x * (1.0f + delta);
                            else if (manipDrag.handle == 1) selectedElem->transform.scale.y = s.y * (1.0f + delta);
                            else selectedElem->transform.scale.z = s.z * (1.0f + delta);
                            selectedElem->transform.scale = glm::max(selectedElem->transform.scale, glm::vec3(0.01f));
                            glm::vec3 localCenter = selectedElem->getLocalBoundsCenter();
                            glm::vec3 offset = glm::vec3(selectedElem->transform.rotation * glm::vec4(selectedElem->transform.scale * localCenter, 0.0f));
                            selectedElem->transform.position = manipDrag.initialGizmoCenter - offset;
                        }
                    }
                }
            }

            // Render to framebuffer
            viewport.beginFrame();
            viewport.renderGrid(25.0f, 100);
            viewport.renderScene(&scene);
            viewport.renderBeams(&scene, scene.getSelectedBeam());
            
            // Render preview beam if drawing (using the previewBeam declared earlier)
            if (currentTool == opticsketch::ToolMode::DrawBeam && beamStartPlaced && previewBeamPtr) {
                viewport.renderBeam(*previewBeamPtr);
            }
            
            // Render gizmo if element is selected and tool is not Select (hovered axis highlighted from previous frame)
            if (scene.getSelectedElement() && toolboxPanel.getCurrentTool() != opticsketch::ToolMode::Select) {
                // Only show single axis while actually dragging; when user releases, show all axes again
                if (!app.input.leftMouseDown) manipDrag.active = false;
                int exclusiveHandle = -1;
                if (manipDrag.active && app.input.leftMouseDown) exclusiveHandle = manipDrag.handle;
                opticsketch::GizmoType gizmoType;
                switch (toolboxPanel.getCurrentTool()) {
                    case opticsketch::ToolMode::Move:
                        gizmoType = opticsketch::GizmoType::Move;
                        break;
                    case opticsketch::ToolMode::Rotate:
                        gizmoType = opticsketch::GizmoType::Rotate;
                        break;
                    case opticsketch::ToolMode::Scale:
                        gizmoType = opticsketch::GizmoType::Scale;
                        break;
                    default:
                        gizmoType = opticsketch::GizmoType::Move;
                        break;
                }
                viewport.renderGizmo(&scene, gizmoType, lastGizmoHoveredHandle, exclusiveHandle);
            }
            
            viewport.endFrame();
            
            // Set up drag-drop target on the image (must be after Image call)
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("LIBRARY_ITEM")) {
                    if (payload->DataSize == sizeof(int)) {
                        int itemIndex = *(const int*)payload->Data;
                        const auto& libItems = libraryPanel.getItems();
                        if (itemIndex >= 0 && itemIndex < static_cast<int>(libItems.size())) {
                            const auto& libItem = libItems[itemIndex];
                            std::unique_ptr<opticsketch::Element> elem;
                            if (libItem.type == opticsketch::ElementType::ImportedMesh && !libItem.meshPath.empty()) {
                                elem = opticsketch::createMeshElement(libItem.meshPath);
                            } else {
                                elem = opticsketch::createElement(libItem.type);
                            }
                            if (elem) {
                                // Ray-cast from viewport center to Y=0 ground plane
                                glm::vec3 dropPos(0.0f, 0.0f, 0.0f);
                                float centerX = vpWidth * 0.5f;
                                float centerY = vpHeight * 0.5f;
                                opticsketch::Raycast::Ray dropRay = opticsketch::Raycast::screenToRay(
                                    viewport.getCamera(), centerX, centerY, vpWidth, vpHeight);
                                if (std::abs(dropRay.direction.y) > 1e-5f) {
                                    float t = -dropRay.origin.y / dropRay.direction.y;
                                    if (t > 0.0f) {
                                        dropPos = dropRay.origin + t * dropRay.direction;
                                        dropPos.y = 0.0f;
                                    }
                                }
                                elem->transform.position = dropPos;
                                scene.addElement(std::move(elem));
                                auto* added = scene.getElements().back().get();
                                undoStack.push(std::make_unique<opticsketch::AddElementCmd>(*added));
                                scene.selectElement(added->id);
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
            
            // Draw selection rectangle while dragging: from start (press position) to current mouse; clamp current to viewport so rect stays visible
            if (selectionBoxActive && app.input.leftMouseDown && !manipDrag.active) {
                float curX = std::max(0.0f, std::min(viewportSize.x, viewportX));
                float curY = std::max(0.0f, std::min(viewportSize.y, viewportY));
                float minX = std::min(selectionBoxStartX, curX);
                float maxX = std::max(selectionBoxStartX, curX);
                float minY = std::min(selectionBoxStartY, curY);
                float maxY = std::max(selectionBoxStartY, curY);
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 pmin(imageMin.x + minX, imageMin.y + minY);
                ImVec2 pmax(imageMin.x + maxX, imageMin.y + maxY);
                dl->AddRectFilled(pmin, pmax, IM_COL32(255, 255, 255, 25));
                dl->AddRect(pmin, pmax, IM_COL32(255, 255, 255, 200), 0.0f, 0, 2.0f);
            }
            
            bool shouldDrag = isViewportHovered && ctrlPressed && !manipDrag.active &&
                             (app.input.leftMouseDown || app.input.middleMouseDown || app.input.rightMouseDown);
            
            // Start drag: capture initial position
            if (shouldDrag && !app.input.isDraggingCamera) {
                app.input.isDraggingCamera = true;
                app.input.dragStartX = mouseX;
                app.input.dragStartY = mouseY;
                app.input.lastMouseX = mouseX;
                app.input.lastMouseY = mouseY;
            }
            
            // Calculate delta only when actively dragging
            double deltaX = 0.0;
            double deltaY = 0.0;
            if (app.input.isDraggingCamera && shouldDrag) {
                deltaX = mouseX - app.input.lastMouseX;
                deltaY = mouseY - app.input.lastMouseY;
                
                // Apply camera movement
                if (app.input.leftMouseDown) {
                    // CTRL + Left Mouse = Rotate (Orbit) - Blender/Maya style
                    if (std::abs(deltaX) > 0.001 || std::abs(deltaY) > 0.001) {
                        viewport.getCamera().orbit(static_cast<float>(deltaX), static_cast<float>(deltaY));
                    }
                } else if (app.input.middleMouseDown) {
                    // CTRL + Middle Mouse = Pan - Blender/Maya style
                    if (std::abs(deltaX) > 0.001 || std::abs(deltaY) > 0.001) {
                        viewport.getCamera().pan(static_cast<float>(deltaX), static_cast<float>(deltaY));
                    }
                } else if (app.input.rightMouseDown) {
                    // CTRL + Right Mouse = Zoom (based on vertical movement)
                    if (std::abs(deltaY) > 0.001) {
                        // Zoom: mouse down = zoom in, mouse up = zoom out
                        float zoomDelta = static_cast<float>(-deltaY) * 0.05f;
                        viewport.getCamera().zoom(zoomDelta);
                    }
                }
                
                // Update mouse position for next frame
                app.input.lastMouseX = mouseX;
                app.input.lastMouseY = mouseY;
            }
            
            // Stop dragging if conditions no longer met
            if (!shouldDrag) {
                app.input.isDraggingCamera = false;
            }
        }
            ImGui::End();
        }
        
        ImGui::Render();
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(0.05f, 0.05f, 0.06f, 1.0f);  // Match theme background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Multi-viewport support
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup);
        }
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup - must happen before destroying OpenGL context
    // Make sure OpenGL context is still current
    glfwMakeContextCurrent(window);
    
    // Explicitly clean up viewport resources while context is still valid
    // This ensures OpenGL resources are freed before context is destroyed
    viewport.cleanup();
    
    // Now shutdown ImGui (this may use OpenGL, so do it before destroying context)
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Unbind any remaining OpenGL objects
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
    
    // Finally destroy window and terminate GLFW
    // This destroys the OpenGL context
    glfwDestroyWindow(window);
    glfwTerminate();
    
    return 0;
}
