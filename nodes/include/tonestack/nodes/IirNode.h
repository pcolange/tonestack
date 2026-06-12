#pragma once

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <vector>

#include "tonestack/Node.h"
#include "tonestack/RtAssert.h"
#include "tonestack/nodes/IirCoeffs.h"
#include "tonestack/nodes/TableInterp.h"

namespace tonestack::nodes {

// A transposed Direct Form II IIR of fixed low order whose coefficients come from a
// generated grid indexed by one bound parameter per table axis. Each block the parameters
// are snapshotted and the coefficient row is multilinearly interpolated between the 2^N
// bracketing grid corners; no allocation after prepare(). The direct (unfactored) form is
// used where a section split cannot be interpolated continuously — see IirCoeffs.h.
class IirNode : public Node {
public:
    IirNode(std::initializer_list<ParameterDesc> paramDescs, const IirCoeffTable& table)
        : table_(table) {
        if (table_.numAxes < 1 || table_.numAxes > kMaxTableAxes)
            throw std::invalid_argument("IirNode: table axis count out of range");
        if (table_.order < 1 || table_.order > kMaxIirOrder)
            throw std::invalid_argument("IirNode: table order out of range");
        if (static_cast<int>(paramDescs.size()) != table_.numAxes)
            throw std::invalid_argument("IirNode: one parameter required per table axis");
        for (const auto& d : paramDescs)
            params_.add(d);
    }

    IirNode(const ParameterDesc& paramDesc, const IirCoeffTable& table)
        : IirNode({paramDesc}, table) {}

    NodeInfo info() const noexcept override { return {"iir"}; }

    void prepare(const ProcessSpec& spec) override {
        if (table_.sampleRate > 0.0 &&
            std::llround(table_.sampleRate) != std::llround(spec.sampleRate))
            throw std::invalid_argument(
                "IirNode: coefficient table sample rate does not match prepare() rate");
        params_.prepare(spec.sampleRate);
        state_.assign(
            static_cast<size_t>(spec.numChannels) * static_cast<size_t>(table_.order), 0.0f);
        interpolate(current_);
    }

    void reset() noexcept override {
        params_.reset();
        std::fill(state_.begin(), state_.end(), 0.0f);
        interpolate(current_);
    }

    void process(AudioBlock& io) noexcept override {
        params_.snapshotBlock(io.numFrames());
        interpolate(current_);
        const int order = table_.order;
        const float* b = current_;             // b0..bN
        const float* a = current_ + order + 1; // a1..aN

        const int maxChannels = static_cast<int>(state_.size()) / order;
        TS_RT_ASSERT(io.numChannels() <= maxChannels);
        const int channels = std::min(io.numChannels(), maxChannels);
        for (int ch = 0; ch < channels; ++ch) {
            float* x = io.channel(ch);
            float* z = state_.data() + static_cast<size_t>(ch) * static_cast<size_t>(order);
            for (int i = 0; i < io.numFrames(); ++i) {
                const float in = x[i];
                const float y = b[0] * in + z[0];
                for (int k = 0; k < order - 1; ++k)
                    z[k] = b[k + 1] * in - a[k] * y + z[k + 1];
                z[order - 1] = b[order] * in - a[order - 1] * y;
                x[i] = y;
            }
        }
    }

    ParameterSet& parameters() noexcept override { return params_; }

private:
    // Multilinear interpolation of the coefficient row at the bound parameters' values.
    void interpolate(float* out) const noexcept {
        TS_RT_ASSERT(table_.coeffs != nullptr && table_.axes != nullptr &&
                     table_.dims != nullptr && table_.numAxes >= 1);
        const int n = table_.numAxes;
        const int width = 2 * table_.order + 1;

        int   i0[kMaxTableAxes];
        float t[kMaxTableAxes];
        int   stride[kMaxTableAxes];
        for (int k = 0; k < n; ++k)
            detail::bracketAxis(table_.axes[k], table_.dims[k], params_.byIndex(k).value(),
                                i0[k], t[k]);
        stride[n - 1] = 1;
        for (int k = n - 2; k >= 0; --k)
            stride[k] = stride[k + 1] * table_.dims[k + 1];

        for (int w = 0; w < width; ++w)
            out[w] = 0.0f;
        const int corners = 1 << n;
        for (int c = 0; c < corners; ++c) {
            float weight = 1.0f;
            int idx = 0;
            for (int k = 0; k < n; ++k) {
                const int bit = (c >> k) & 1;
                weight *= bit != 0 ? t[k] : 1.0f - t[k];
                const int gi = std::min(i0[k] + bit, table_.dims[k] - 1);
                idx += stride[k] * gi;
            }
            if (weight == 0.0f)
                continue;
            const float* row =
                table_.coeffs + static_cast<size_t>(idx) * static_cast<size_t>(width);
            for (int w = 0; w < width; ++w)
                out[w] += weight * row[w];
        }
    }

    IirCoeffTable table_{};
    ParameterSet params_;

    float current_[2 * kMaxIirOrder + 1] = {};
    std::vector<float> state_; // [channels][order], sized in prepare()
};

} // namespace tonestack::nodes
