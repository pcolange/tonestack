#pragma once

#include <string_view>

namespace tonestack {

// Static descriptor a Node advertises about itself.
struct NodeInfo {
    std::string_view typeId; // e.g. "gain", "biquad", "module.tubescreamer"
};

} // namespace tonestack
