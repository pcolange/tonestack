#pragma once

#include <atomic>
#include <string>

namespace tonestack {

enum class ParamSkew {
    Linear,
    Logarithmic // log taper (emulates a real pot), value==skewMidpoint at proportion 0.5
};

// Static description of a parameter. `min`/`max` are the physical range (e.g. a pot
// fraction, a gain); the host/UI works in a normalized [0,1] proportion that maps onto
// that range through `skew`.
struct ParameterDesc {
    std::string id;
    float min = 0.0f;
    float max = 1.0f;
    float defaultProportion = 0.0f;   // normalized [0,1]
    ParamSkew skew = ParamSkew::Linear;
    float skewMidpoint = 0.5f;        // physical value reached at proportion 0.5 (log skew)
    float smoothingSeconds = 0.0f;    // 0 == snap instantly each block
};

// One parameter. The host/UI thread writes a normalized target via setProportion();
// the audio thread takes a single snapshot per block in snapshotBlock(numFrames) and reads
// the smoothed physical value via value(). Smoothing time is wall-clock — derived from the
// actual frames delivered, not the prepared maximum, so variable host block sizes do not
// stretch it. Update is lock-free (a relaxed atomic store/load).
class Parameter {
public:
    Parameter() = default;
    explicit Parameter(const ParameterDesc& desc);

    // std::atomic is not copyable/movable, so these transfer its value explicitly. Only
    // used during setup (e.g. vector growth), never on the audio thread.
    Parameter(const Parameter& other) noexcept;
    Parameter(Parameter&& other) noexcept;
    Parameter& operator=(const Parameter& other) noexcept;
    Parameter& operator=(Parameter&& other) noexcept;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // host / UI thread
    void setProportion(float proportion01) noexcept;
    float targetProportion() const noexcept;

    // audio thread
    void  snapshotBlock(int numFrames) noexcept;
    float value() const noexcept;       // smoothed physical value (range + skew applied)
    float proportion() const noexcept;  // smoothed normalized value

    const ParameterDesc& desc() const noexcept { return desc_; }

private:
    float convertFrom0to1(float proportion01) const noexcept;

    ParameterDesc desc_{};
    float skewFactor_ = 1.0f;           // precomputed from skewMidpoint

    std::atomic<float> target_{0.0f};   // normalized [0,1], written by UI/host thread
    float current_ = 0.0f;              // smoothed normalized, audio thread only
    double sampleRate_ = 0.0;           // for the per-block smoothing coefficient
};

} // namespace tonestack
