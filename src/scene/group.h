#pragma once

#include <string>
#include <vector>

namespace opticsketch {

struct Group {
    std::string id;
    std::string name;
    std::vector<std::string> memberIds;

    static std::string generateId();
};

} // namespace opticsketch
