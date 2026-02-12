#pragma once

#include <string>

namespace opticsketch {

class Scene;
struct SceneStyle;

bool saveProject(const std::string& path, Scene* scene, SceneStyle* style = nullptr);
bool loadProject(const std::string& path, Scene* scene, SceneStyle* style = nullptr);

} // namespace opticsketch
