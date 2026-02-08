#pragma once

#include <string>

namespace opticsketch {

class Scene;

bool saveProject(const std::string& path, Scene* scene);
bool loadProject(const std::string& path, Scene* scene);

} // namespace opticsketch
