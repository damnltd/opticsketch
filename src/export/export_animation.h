#pragma once

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace opticsketch {

class Scene;
class Viewport;
struct SceneStyle;

enum class AnimationType {
    Turntable,
    BeamPropagation,
    ParameterSweep
};

enum class AnimationOutputFormat {
    ImageSequence,
    GIF,
    MP4
};

enum class EasingFunction {
    Linear,
    EaseInOut,
    EaseIn,
    EaseOut
};

struct TurntableParams {
    float startAngle = 0.0f;    // degrees
    float endAngle = 360.0f;    // degrees
    float elevation = 30.0f;    // degrees
    float distance = 0.0f;      // 0 = auto-compute from scene bounds
};

struct BeamPropagationParams {
    bool allBeams = true;
    std::string beamId;         // specific beam if !allBeams
};

struct ParameterSweepParams {
    std::string elementId;
    int parameterIndex = 0;     // 0=posX, 1=posY, 2=posZ, 3=rotY, 4=focalLength
    float startValue = 0.0f;
    float endValue = 1.0f;
};

struct AnimationExportSettings {
    AnimationType type = AnimationType::Turntable;
    AnimationOutputFormat format = AnimationOutputFormat::ImageSequence;
    int frameCount = 120;
    int fps = 30;
    int width = 1280;
    int height = 720;
    EasingFunction easing = EasingFunction::Linear;
    std::string outputPath;

    TurntableParams turntable;
    BeamPropagationParams beamPropagation;
    ParameterSweepParams parameterSweep;
};

struct AnimationExportState {
    bool active = false;
    int currentFrame = 0;
    int totalFrames = 0;
    bool cancelled = false;
    std::string statusText;

    // Saved state for restore after export
    float savedAzimuth = 0.0f;
    float savedElevation = 0.0f;
    float savedDistance = 0.0f;
    glm::vec3 savedTarget{0.0f};

    // For beam propagation: saved beam endpoints
    struct BeamSnapshot {
        std::string id;
        glm::vec3 originalEnd;
    };
    std::vector<BeamSnapshot> beamSnapshots;
};

// Apply easing function to normalized progress [0,1]
float applyEasing(float t, EasingFunction easing);

// Begin animation export (saves camera state, prepares state)
void beginAnimationExport(AnimationExportState& state, const AnimationExportSettings& settings,
                          Viewport* viewport, Scene* scene);

// Advance one frame of the animation. Returns true if more frames remain.
bool advanceAnimationFrame(AnimationExportState& state, const AnimationExportSettings& settings,
                           Viewport* viewport, Scene* scene, SceneStyle* style);

// End animation export (restore camera, finalize files)
void endAnimationExport(AnimationExportState& state, const AnimationExportSettings& settings,
                        Viewport* viewport, Scene* scene);

// Check if ffmpeg is available on the system
bool isFFmpegAvailable();

} // namespace opticsketch
