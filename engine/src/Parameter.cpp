#include "tonestack/Parameter.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace tonestack {

Parameter::Parameter(const Parameter& o) noexcept
    : desc_(o.desc_), skewFactor_(o.skewFactor_),
      target_(o.target_.load(std::memory_order_relaxed)),
      current_(o.current_), sampleRate_(o.sampleRate_) {}

Parameter::Parameter(Parameter&& o) noexcept
    : desc_(std::move(o.desc_)), skewFactor_(o.skewFactor_),
      target_(o.target_.load(std::memory_order_relaxed)),
      current_(o.current_), sampleRate_(o.sampleRate_) {}

Parameter& Parameter::operator=(const Parameter& o) noexcept {
    if (this != &o) {
        desc_ = o.desc_;
        skewFactor_ = o.skewFactor_;
        target_.store(o.target_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        current_ = o.current_;
        sampleRate_ = o.sampleRate_;
    }
    return *this;
}

Parameter& Parameter::operator=(Parameter&& o) noexcept {
    if (this != &o) {
        desc_ = std::move(o.desc_);
        skewFactor_ = o.skewFactor_;
        target_.store(o.target_.load(std::memory_order_relaxed), std::memory_order_relaxed);
        current_ = o.current_;
        sampleRate_ = o.sampleRate_;
    }
    return *this;
}

Parameter::Parameter(const ParameterDesc& desc) : desc_(desc) {
    // Log-skew factor chosen so that value(proportion 0.5) == skewMidpoint.
    if (desc_.skew == ParamSkew::Logarithmic) {
        const float span = desc_.max - desc_.min;
        if (span > 0.0f) {
            const float midFrac = (desc_.skewMidpoint - desc_.min) / span;
            if (midFrac > 0.0f && midFrac < 1.0f)
                skewFactor_ = std::log(0.5f) / std::log(midFrac);
        }
    }
    target_.store(desc_.defaultProportion, std::memory_order_relaxed);
    current_ = desc_.defaultProportion;
}

void Parameter::prepare(double sampleRate) noexcept {
    sampleRate_ = sampleRate;
    reset();
}

void Parameter::reset() noexcept {
    current_ = target_.load(std::memory_order_relaxed);
}

void Parameter::setProportion(float proportion01) noexcept {
    target_.store(std::clamp(proportion01, 0.0f, 1.0f), std::memory_order_relaxed);
}

float Parameter::targetProportion() const noexcept {
    return target_.load(std::memory_order_relaxed);
}

void Parameter::snapshotBlock(int numFrames) noexcept {
    const float t = target_.load(std::memory_order_relaxed);
    if (desc_.smoothingSeconds <= 0.0f || sampleRate_ <= 0.0 || numFrames <= 0) {
        current_ = t; // snap instantly
        return;
    }
    const double blockSeconds = static_cast<double>(numFrames) / sampleRate_;
    const double tau = static_cast<double>(desc_.smoothingSeconds);
    const float coeff = static_cast<float>(1.0 - std::exp(-blockSeconds / tau));
    current_ += coeff * (t - current_);
}

float Parameter::proportion() const noexcept {
    return current_;
}

float Parameter::value() const noexcept {
    return convertFrom0to1(current_);
}

float Parameter::convertFrom0to1(float proportion01) const noexcept {
    float p = std::clamp(proportion01, 0.0f, 1.0f);
    if (desc_.skew == ParamSkew::Logarithmic && skewFactor_ != 1.0f && p > 0.0f)
        p = std::pow(p, 1.0f / skewFactor_);
    return desc_.min + (desc_.max - desc_.min) * p;
}

} // namespace tonestack
