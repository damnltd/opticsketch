#pragma once

#include <string>

namespace opticsketch {

class Scene;
struct SceneStyle;

bool saveProject(const std::string& path, Scene* scene, SceneStyle* style = nullptr);
bool loadProject(const std::string& path, Scene* scene, SceneStyle* style = nullptr);

bool saveStylePreset(const std::string& path, const SceneStyle* style);
bool loadStylePreset(const std::string& path, SceneStyle* style);

} // namespace opticsketch
