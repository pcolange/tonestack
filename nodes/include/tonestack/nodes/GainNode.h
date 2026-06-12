#pragma once

#include "tonestack/Node.h"

namespace tonestack::nodes {

// Simplest possible Node: a smoothed output gain. Serves as the Phase 0 proof that the
// Node / ParameterSet / Chain contract works end to end.
class GainNode : public Node {
public:
    GainNode() {
        params_.define({ParameterDesc{
            /*id*/ "gain",
            /*min*/ 0.0f, /*max*/ 1.0f,
            /*defaultProportion*/ 0.5f,
            ParamSkew::Linear,
            /*skewMidpoint*/ 0.5f,
            /*smoothingSeconds*/ 0.02f}});
        gainIndex_ = params_.indexOf("gain");
    }

    NodeInfo info() const noexcept override { return {"gain"}; }

    void prepare(const ProcessSpec& spec) override {
        params_.prepare(spec.sampleRate);
    }

    void reset() noexcept override { params_.reset(); }

    void process(AudioBlock& io) noexcept override {
        params_.snapshotBlock(io.numFrames());
        const float g = params_.byIndex(gainIndex_).value();
        for (int ch = 0; ch < io.numChannels(); ++ch) {
            float* x = io.channel(ch);
            for (int i = 0; i < io.numFrames(); ++i)
                x[i] *= g;
        }
    }

    ParameterSet& parameters() noexcept override { return params_; }

private:
    ParameterSet params_;
    int gainIndex_ = -1;
};

} // namespace tonestack::nodes
