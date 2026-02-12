#include "scene/group.h"
#include <atomic>
#include <cstdio>

namespace opticsketch {

static std::atomic<int> s_groupCounter{1};

std::string Group::generateId() {
    char buf[32];
    snprintf(buf, sizeof(buf), "grp_%04d", s_groupCounter.fetch_add(1));
    return std::string(buf);
}

} // namespace opticsketch
