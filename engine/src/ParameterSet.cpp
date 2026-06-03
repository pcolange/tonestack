#include "tonestack/ParameterSet.h"

#include <cassert>

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

void ParameterSet::prepare(double sampleRate, int maxBlockSize) noexcept {
    for (auto& p : params_)
        p.prepare(sampleRate, maxBlockSize);
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
    assert(i >= 0 && "ParameterSet::get: unknown parameter id");
    return params_[static_cast<size_t>(i)];
}

void ParameterSet::snapshotBlock() noexcept {
    for (auto& p : params_)
        p.snapshotBlock();
}

} // namespace tonestack
