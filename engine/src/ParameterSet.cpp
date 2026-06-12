#include "tonestack/ParameterSet.h"

#include <stdexcept>
#include <string>

namespace tonestack {

void ParameterSet::define(std::initializer_list<ParameterDesc> descs) {
    params_.clear();
    params_.reserve(descs.size());
    for (const auto& d : descs)
        params_.emplace_back(d);
}

void ParameterSet::add(const ParameterDesc& desc) {
    params_.emplace_back(desc);
}

void ParameterSet::prepare(double sampleRate) noexcept {
    for (auto& p : params_)
        p.prepare(sampleRate);
}

void ParameterSet::reset() noexcept {
    for (auto& p : params_)
        p.reset();
}

int ParameterSet::indexOf(std::string_view id) const noexcept {
    for (size_t i = 0; i < params_.size(); ++i)
        if (params_[i].desc().id == id)
            return static_cast<int>(i);
    return -1;
}

Parameter& ParameterSet::get(std::string_view id) {
    const int i = indexOf(id);
    if (i < 0)
        throw std::out_of_range("ParameterSet::get: unknown parameter id: " + std::string(id));
    return params_[static_cast<size_t>(i)];
}

void ParameterSet::snapshotBlock(int numFrames) noexcept {
    for (auto& p : params_)
        p.snapshotBlock(numFrames);
}

} // namespace tonestack
