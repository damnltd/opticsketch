#include "export/export_animation.h"
#include "render/viewport.h"
#include "scene/scene.h"
#include "render/beam.h"
#include "camera/camera.h"
#include "style/scene_style.h"
#include "elements/element.h"
#include "export/export_png.h"
#include <glad/glad.h>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace opticsketch {

float applyEasing(float t, EasingFunction easing) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easing) {
        case EasingFunction::EaseIn:
            return t * t;
        case EasingFunction::EaseOut:
            return 1.0f - (1.0f - t) * (1.0f - t);
        case EasingFunction::EaseInOut:
            return t < 0.5f ? 2.0f * t * t : 1.0f - 0.5f * (2.0f - 2.0f * t) * (2.0f - 2.0f * t);
        case EasingFunction::Linear:
        default:
            return t;
    }
}

bool isFFmpegAvailable() {
#ifdef _WIN32
    int ret = std::system("where ffmpeg >nul 2>nul");
#else
    int ret = std::system("which ffmpeg >/dev/null 2>/dev/null");
#endif
    return ret == 0;
}

// Helper: get temp directory for intermediate frames (GIF/MP4 formats)
static std::string getTempFrameDir(const std::string& outputPath) {
    namespace fs = std::filesystem;
    fs::path p(outputPath);
    fs::path tempDir = p.parent_path() / ".opticsketch_anim_tmp";
    fs::create_directories(tempDir);
    return tempDir.string();
}

void beginAnimationExport(AnimationExportState& state, const AnimationExportSettings& settings,
                          Viewport* viewport, Scene* scene) {
    state.active = true;
    state.currentFrame = 0;
    state.totalFrames = settings.frameCount;
    state.cancelled = false;
    state.statusText = "Starting export...";

    // Save camera state
    Camera& cam = viewport->getCamera();
    state.savedAzimuth = cam.getAzimuth();
    state.savedElevation = cam.getElevation();
    state.savedDistance = cam.getDistance();
    state.savedTarget = cam.target;

    // Save beam endpoints for beam propagation
    state.beamSnapshots.clear();
    if (settings.type == AnimationType::BeamPropagation) {
        for (const auto& beam : scene->getBeams()) {
            if (!beam->visible) continue;
            if (!settings.beamPropagation.allBeams && beam->id != settings.beamPropagation.beamId)
                continue;
            state.beamSnapshots.push_back({beam->id, beam->end});
        }
    }

    // Create output directory
    if (settings.format == AnimationOutputFormat::ImageSequence) {
        std::filesystem::create_directories(settings.outputPath);
    } else {
        // GIF/MP4: create temp directory for intermediate PNG frames
        getTempFrameDir(settings.outputPath);
    }
}

bool advanceAnimationFrame(AnimationExportState& state, const AnimationExportSettings& settings,
                           Viewport* viewport, Scene* scene, SceneStyle* style) {
    if (!state.active || state.cancelled) return false;
    if (state.currentFrame >= state.totalFrames) return false;

    float progress = (state.totalFrames > 1) ?
        static_cast<float>(state.currentFrame) / static_cast<float>(state.totalFrames - 1) : 1.0f;
    float easedProgress = applyEasing(progress, settings.easing);

    Camera& cam = viewport->getCamera();

    switch (settings.type) {
        case AnimationType::Turntable: {
            float startRad = glm::radians(settings.turntable.startAngle);
            float endRad = glm::radians(settings.turntable.endAngle);
            float elevRad = glm::radians(settings.turntable.elevation);
            float azimuth = startRad + (endRad - startRad) * easedProgress;
            float dist = settings.turntable.distance;
            if (dist <= 0.0f) dist = state.savedDistance;
            cam.setSpherical(azimuth, elevRad, dist);
            break;
        }
        case AnimationType::BeamPropagation: {
            for (const auto& snap : state.beamSnapshots) {
                for (const auto& beam : scene->getBeams()) {
                    if (beam->id == snap.id) {
                        beam->end = beam->start + (snap.originalEnd - beam->start) * easedProgress;
                        break;
                    }
                }
            }
            break;
        }
        case AnimationType::ParameterSweep: {
            auto* elem = scene->getElement(settings.parameterSweep.elementId);
            if (elem) {
                float val = settings.parameterSweep.startValue +
                    (settings.parameterSweep.endValue - settings.parameterSweep.startValue) * easedProgress;
                switch (settings.parameterSweep.parameterIndex) {
                    case 0: elem->transform.position.x = val; break;
                    case 1: elem->transform.position.y = val; break;
                    case 2: elem->transform.position.z = val; break;
                    case 3: {
                        glm::vec3 up(0.0f, 1.0f, 0.0f);
                        elem->transform.rotation = glm::angleAxis(glm::radians(val), up);
                        break;
                    }
                    case 4: elem->optics.focalLength = val; break;
                    default: break;
                }
            }
            break;
        }
    }

    // Render the frame to the viewport's FBO
    viewport->beginFrame();
    viewport->renderGrid(25.0f, 100);
    viewport->renderScene(scene, true); // forExport=true
    viewport->renderBeams(scene);
    viewport->renderGaussianBeams(scene);
    viewport->renderFocalPoints(scene);
    viewport->endFrame();

    if (style && style->renderMode == RenderMode::Presentation) {
        viewport->renderBloomPass();
    }

    // Read pixels from the viewport texture (bottom-up OpenGL order)
    int w = viewport->getWidth();
    int h = viewport->getHeight();
    std::vector<unsigned char> pixels(w * h * 3);
    glBindTexture(GL_TEXTURE_2D, viewport->getTextureId());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);

    // Build frame path
    std::string frameDir;
    if (settings.format == AnimationOutputFormat::ImageSequence) {
        frameDir = settings.outputPath;
    } else {
        frameDir = getTempFrameDir(settings.outputPath);
    }

    std::ostringstream framePath;
    framePath << frameDir << "/frame_"
              << std::setfill('0') << std::setw(5) << state.currentFrame << ".png";

    // savePngToFile handles the vertical flip internally
    savePngToFile(framePath.str(), w, h, pixels.data());

    state.currentFrame++;
    std::ostringstream ss;
    ss << "Frame " << state.currentFrame << " / " << state.totalFrames;
    state.statusText = ss.str();

    return state.currentFrame < state.totalFrames;
}

void endAnimationExport(AnimationExportState& state, const AnimationExportSettings& settings,
                        Viewport* viewport, Scene* scene) {
    // Restore camera
    Camera& cam = viewport->getCamera();
    cam.setSpherical(state.savedAzimuth, state.savedElevation, state.savedDistance);
    cam.target = state.savedTarget;

    // Restore beam endpoints
    for (const auto& snap : state.beamSnapshots) {
        for (const auto& beam : scene->getBeams()) {
            if (beam->id == snap.id) {
                beam->end = snap.originalEnd;
                break;
            }
        }
    }

    // Assemble GIF or MP4 from temporary PNG frames using ffmpeg
    if (!state.cancelled && settings.format != AnimationOutputFormat::ImageSequence && isFFmpegAvailable()) {
        std::string tempDir = getTempFrameDir(settings.outputPath);
        std::ostringstream cmd;

        if (settings.format == AnimationOutputFormat::GIF) {
            // Generate GIF with palette for good quality
            std::string palettePath = tempDir + "/palette.png";
            // Pass 1: generate palette
            cmd << "ffmpeg -y -framerate " << settings.fps
                << " -i \"" << tempDir << "/frame_%05d.png\""
                << " -vf \"palettegen\" \"" << palettePath << "\"";
            std::system(cmd.str().c_str());
            // Pass 2: encode GIF using palette
            cmd.str("");
            cmd << "ffmpeg -y -framerate " << settings.fps
                << " -i \"" << tempDir << "/frame_%05d.png\""
                << " -i \"" << palettePath << "\""
                << " -lavfi \"paletteuse\" \"" << settings.outputPath << "\"";
            std::system(cmd.str().c_str());
        } else if (settings.format == AnimationOutputFormat::MP4) {
            cmd << "ffmpeg -y -framerate " << settings.fps
                << " -i \"" << tempDir << "/frame_%05d.png\""
                << " -c:v libx264 -crf 18 -pix_fmt yuv420p"
                << " \"" << settings.outputPath << "\"";
            std::system(cmd.str().c_str());
        }

        // Clean up temp directory
        std::filesystem::remove_all(tempDir);
    }

    state.active = false;
    state.statusText = state.cancelled ? "Export cancelled" : "Export complete";
}

} // namespace opticsketch
