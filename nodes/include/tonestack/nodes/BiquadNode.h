#pragma once

#include <algorithm>
#include <vector>

#include "tonestack/Node.h"
#include "tonestack/RtAssert.h"
#include "tonestack/nodes/BiquadCoeffs.h"

namespace tonestack::nodes {

// A transposed Direct Form II biquad whose coefficients come from a generated table indexed
// by one bound parameter (a pot fraction). Each block the parameter is snapshotted and the
// section is linearly interpolated between the two bracketing table rows; the engine never
// evaluates a coefficient expression and never allocates after prepare().
//
// The table is injected as a non-owning view of constexpr data, so this node has no
// compile-time dependency on the compiler-generated header.
class BiquadNode : public Node {
public:
    // `paramDesc` defines the pot parameter; its physical value() indexes `table.axis`.
    BiquadNode(const ParameterDesc& paramDesc, const BiquadCoeffTable& table)
        : table_(table) {
        params_.add(paramDesc);
        paramIndex_ = params_.indexOf(paramDesc.id);
    }

    NodeInfo info() const noexcept override { return {"biquad", {}, {}}; }

    void prepare(const ProcessSpec& spec) override {
        params_.prepare(spec.sampleRate, spec.maxBlockSize);
        z1_.assign(static_cast<size_t>(spec.numChannels), 0.0f);
        z2_.assign(static_cast<size_t>(spec.numChannels), 0.0f);
        coeffsForValue(currentAxisValue(), current_);
    }

    void reset() noexcept override {
        params_.reset();
        std::fill(z1_.begin(), z1_.end(), 0.0f);
        std::fill(z2_.begin(), z2_.end(), 0.0f);
        coeffsForValue(currentAxisValue(), current_);
    }

    void process(AudioBlock& io) noexcept override {
        params_.snapshotBlock();
        coeffsForValue(currentAxisValue(), current_);
        const BiquadSection c = current_;

        const int channels = std::min(io.numChannels(), static_cast<int>(z1_.size()));
        for (int ch = 0; ch < channels; ++ch) {
            float* x = io.channel(ch);
            float z1 = z1_[static_cast<size_t>(ch)];
            float z2 = z2_[static_cast<size_t>(ch)];
            for (int i = 0; i < io.numFrames(); ++i) {
                const float in = x[i];
                const float y = c.b0 * in + z1;
                z1 = c.b1 * in - c.a1 * y + z2;
                z2 = c.b2 * in - c.a2 * y;
                x[i] = y;
            }
            z1_[static_cast<size_t>(ch)] = z1;
            z2_[static_cast<size_t>(ch)] = z2;
        }
    }

    ParameterSet& parameters() noexcept override { return params_; }

private:
    float currentAxisValue() const noexcept {
        return params_.byIndex(paramIndex_).value();
    }

    // Linear interpolation of the section at axis value `v`, clamped to the table range.
    void coeffsForValue(float v, BiquadSection& out) const noexcept {
        TS_RT_ASSERT(table_.rows > 0 && table_.sections != nullptr && table_.axis != nullptr);
        if (table_.rows <= 1) {
            out = table_.sections[0];
            return;
        }
        const float* begin = table_.axis;
        const float* end = table_.axis + table_.rows;
        // First row with axis > v; the bracketing pair is [hi-1, hi].
        const float* hi = std::upper_bound(begin, end, v);
        if (hi == begin) {
            out = table_.sections[0];
            return;
        }
        if (hi == end) {
            out = table_.sections[table_.rows - 1];
            return;
        }
        const auto i1 = static_cast<size_t>(hi - begin);
        const size_t i0 = i1 - 1;
        const float a0 = table_.axis[i0];
        const float a1 = table_.axis[i1];
        const float span = a1 - a0;
        const float t = span > 0.0f ? (v - a0) / span : 0.0f;
        const BiquadSection& s0 = table_.sections[i0];
        const BiquadSection& s1 = table_.sections[i1];
        out.b0 = s0.b0 + t * (s1.b0 - s0.b0);
        out.b1 = s0.b1 + t * (s1.b1 - s0.b1);
        out.b2 = s0.b2 + t * (s1.b2 - s0.b2);
        out.a1 = s0.a1 + t * (s1.a1 - s0.a1);
        out.a2 = s0.a2 + t * (s1.a2 - s0.a2);
    }

    BiquadCoeffTable table_{};
    ParameterSet params_;
    int paramIndex_ = -1;

    BiquadSection current_{};
    std::vector<float> z1_; // per-channel state, sized in prepare()
    std::vector<float> z2_;
};

} // namespace tonestack::nodes
