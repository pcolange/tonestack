#pragma once

#include <string_view>

namespace tonestack {

struct PortSpec {
    int numChannels = 1;
};

// Static descriptor a Node advertises about itself.
struct NodeInfo {
    std::string_view typeId; // e.g. "gain", "biquad", "module.tubescreamer"
    PortSpec input{};
    PortSpec output{};
};

} // namespace tonestack
