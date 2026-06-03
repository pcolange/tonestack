#pragma once

#include <initializer_list>
#include <string_view>
#include <vector>

#include "tonestack/Parameter.h"

namespace tonestack {

// An ordered, typed collection of parameters owned by a Node. Define parameters before
// prepare() (setup/UI thread). On the audio thread, address them by cached index, not by
// string, and snapshot the whole set once per block.
class ParameterSet {
public:
    void define(std::initializer_list<ParameterDesc> descs);
    void add(const ParameterDesc& desc);

    void prepare(double sampleRate, int maxBlockSize) noexcept;
    void reset() noexcept;

    int size() const noexcept { return static_cast<int>(params_.size()); }

    // setup-time lookup (asserts the id exists)
    Parameter& get(std::string_view id);
    int indexOf(std::string_view id) const noexcept; // -1 if absent

    // audio-thread access by cached index
    Parameter&       byIndex(int i)       noexcept { return params_[static_cast<size_t>(i)]; }
    const Parameter& byIndex(int i) const noexcept { return params_[static_cast<size_t>(i)]; }

    void snapshotBlock() noexcept;

private:
    std::vector<Parameter> params_;
};

} // namespace tonestack
