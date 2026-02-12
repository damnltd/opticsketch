#pragma once

#include <string>
#include <vector>

namespace opticsketch {

class Scene;

struct TemplateInfo {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
};

const std::vector<TemplateInfo>& getTemplateList();
void loadTemplate(const std::string& id, Scene* scene);

} // namespace opticsketch
