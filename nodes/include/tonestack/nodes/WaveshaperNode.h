#pragma once

#include "tonestack/Node.h"
#include "tonestack/nodes/WaveshaperShapes.h"

namespace tonestack::nodes {

// Applies a memoryless Shape per sample. Shape is a POD functor
// (`float operator()(float x, float control) const noexcept`) whose constants come from a
// compiler-generated header; an optional bound parameter supplies `control` (snapshotted
// once per block). Stateless apart from the parameter — intended to run inside an
// OversampledNode when the shape adds harmonics.
template <typename Shape>
class WaveshaperNode : public Node {
public:
    WaveshaperNode(const ParameterDesc& controlDesc, const Shape& shape)
        : shape_(shape), hasControl_(true) {
        params_.add(controlDesc);
    }

    explicit WaveshaperNode(const Shape& shape) : shape_(shape), hasControl_(false) {}

    NodeInfo info() const noexcept override { return {"waveshaper"}; }

    void prepare(const ProcessSpec& spec) override { params_.prepare(spec.sampleRate); }

    void reset() noexcept override { params_.reset(); }

    void process(AudioBlock& io) noexcept override {
        params_.snapshotBlock(io.numFrames());
        const float control = hasControl_ ? params_.byIndex(0).value() : 0.0f;
        for (int ch = 0; ch < io.numChannels(); ++ch) {
            float* x = io.channel(ch);
            for (int i = 0; i < io.numFrames(); ++i)
                x[i] = shape_(x[i], control);
        }
    }

    ParameterSet& parameters() noexcept override { return params_; }

private:
    Shape shape_;
    ParameterSet params_;
    bool hasControl_ = false;
};

} // namespace tonestack::nodes
