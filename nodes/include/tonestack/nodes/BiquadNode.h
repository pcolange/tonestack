#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <vector>

#include "tonestack/Node.h"
#include "tonestack/RtAssert.h"
#include "tonestack/nodes/BiquadCoeffs.h"
#include "tonestack/nodes/TableInterp.h"

namespace tonestack::nodes {

// A transposed Direct Form II biquad whose coefficients come from a generated grid indexed
// by one bound parameter per table axis. Each block the parameters are snapshotted and the
// section is multilinearly interpolated between the 2^N bracketing grid corners; the engine
// never evaluates a coefficient expression and never allocates after prepare().
//
// The table is injected as a non-owning view of constexpr data, so this node has no
// compile-time dependency on the compiler-generated header.
class BiquadNode : public Node {
public:
    // One ParameterDesc per table axis, in axis order; each physical value() indexes the
    // matching `table.axes[k]`.
    BiquadNode(std::initializer_list<ParameterDesc> paramDescs, const BiquadCoeffTable& table)
        : table_(table) {
        if (table_.numAxes < 1 || table_.numAxes > kMaxTableAxes)
            throw std::invalid_argument("BiquadNode: table axis count out of range");
        if (static_cast<int>(paramDescs.size()) != table_.numAxes)
            throw std::invalid_argument("BiquadNode: one parameter required per table axis");
        for (const auto& d : paramDescs)
            params_.add(d);
    }

    // Single-axis convenience.
    BiquadNode(const ParameterDesc& paramDesc, const BiquadCoeffTable& table)
        : BiquadNode({paramDesc}, table) {}

    NodeInfo info() const noexcept override { return {"biquad"}; }

    void prepare(const ProcessSpec& spec) override {
        // A table is only valid at the rate it was discretized for; running it at another
        // rate silently detunes the filter, so reject the mismatch loudly here.
        if (table_.sampleRate > 0.0 &&
            std::llround(table_.sampleRate) != std::llround(spec.sampleRate))
            throw std::invalid_argument(
                "BiquadNode: coefficient table sample rate does not match prepare() rate");
        params_.prepare(spec.sampleRate);
        z1_.assign(static_cast<size_t>(spec.numChannels), 0.0f);
        z2_.assign(static_cast<size_t>(spec.numChannels), 0.0f);
        interpolate(current_);
    }

    void reset() noexcept override {
        params_.reset();
        std::fill(z1_.begin(), z1_.end(), 0.0f);
        std::fill(z2_.begin(), z2_.end(), 0.0f);
        interpolate(current_);
    }

    void process(AudioBlock& io) noexcept override {
        params_.snapshotBlock(io.numFrames());
        interpolate(current_);
        const BiquadSection c = current_;

        TS_RT_ASSERT(io.numChannels() <= static_cast<int>(z1_.size()));
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
    // Multilinear interpolation of the section at the bound parameters' current values:
    // gather the 2^N bracketing grid corners (row-major, last axis fastest).
    void interpolate(BiquadSection& out) const noexcept {
        TS_RT_ASSERT(table_.sections != nullptr && table_.axes != nullptr &&
                     table_.dims != nullptr && table_.numAxes >= 1);
        const int n = table_.numAxes;

        int   i0[kMaxTableAxes];
        float t[kMaxTableAxes];
        int   stride[kMaxTableAxes];
        for (int k = 0; k < n; ++k)
            detail::bracketAxis(table_.axes[k], table_.dims[k], params_.byIndex(k).value(),
                                i0[k], t[k]);
        stride[n - 1] = 1;
        for (int k = n - 2; k >= 0; --k)
            stride[k] = stride[k + 1] * table_.dims[k + 1];

        float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        const int corners = 1 << n;
        for (int c = 0; c < corners; ++c) {
            float w = 1.0f;
            int idx = 0;
            for (int k = 0; k < n; ++k) {
                const int bit = (c >> k) & 1;
                w *= bit != 0 ? t[k] : 1.0f - t[k];
                const int gi = std::min(i0[k] + bit, table_.dims[k] - 1);
                idx += stride[k] * gi;
            }
            if (w == 0.0f)
                continue;
            const BiquadSection& s = table_.sections[idx];
            b0 += w * s.b0;
            b1 += w * s.b1;
            b2 += w * s.b2;
            a1 += w * s.a1;
            a2 += w * s.a2;
        }
        out.b0 = b0;
        out.b1 = b1;
        out.b2 = b2;
        out.a1 = a1;
        out.a2 = a2;
    }

    BiquadCoeffTable table_{};
    ParameterSet params_;

    BiquadSection current_{};
    std::vector<float> z1_; // per-channel state, sized in prepare()
    std::vector<float> z2_;
};

} // namespace tonestack::nodes
