#pragma once

#include <memory>
#include <stdexcept>

#include "rangemaster_tables.h"
#include "tonestack/Chain.h"
#include "tonestack/nodes/BiquadNode.h"
#include "tonestack/nodes/IirNode.h"
#include "tonestack/nodes/OversampledNode.h"
#include "tonestack/nodes/WaveshaperNode.h"

namespace tonestack::nodes {

// The Dallas Rangemaster as a composite node: source/input grid (volume, cable, pickup)
// -> x4-oversampled germanium stage -> boost tap. Tables come from the circuitc-generated
// header, so consumers need nodes/generated/ on their include path (codegen-gated targets
// only). Built for a specific sample rate — table selection happens here; rate-flexible
// construction arrives with the plugin shell's parameter-flattening phase, as does a
// unified parameters() (use the control accessors until then).
class RangemasterModule : public Node {
public:
    explicit RangemasterModule(double sampleRate) {
        namespace gen = tonestack::nodes::generated::rangemaster;
        const IirCoeffTable* input = gen::inputTable(sampleRate);
        const BiquadCoeffTable* output = gen::outputTable(sampleRate);
        if (input == nullptr || output == nullptr)
            throw std::invalid_argument("RangemasterModule: unsupported sample rate");

        chain_.add(std::make_unique<IirNode>(
            std::initializer_list<ParameterDesc>{gen::inputParam(0), gen::inputParam(1),
                                                 gen::inputParam(2)},
            *input));
        chain_.add(std::make_unique<OversampledNode>(
            std::make_unique<WaveshaperNode<GeBjtShape>>(gen::stage_shape)));
        chain_.add(std::make_unique<BiquadNode>(gen::outputParam(0), *output));
    }

    NodeInfo info() const noexcept override { return {"module.rangemaster"}; }

    void prepare(const ProcessSpec& spec) override { chain_.prepare(spec); }
    void reset() noexcept override { chain_.reset(); }
    void process(AudioBlock& io) noexcept override { chain_.process(io); }
    int latencySamples() const noexcept override { return chain_.latencySamples(); }

    ParameterSet& parameters() noexcept override { return chain_.parameters(); }

    // Control accessors (setup/UI thread).
    Parameter& volume() { return chain_.at(0).parameters().get("volume"); }
    Parameter& cable() { return chain_.at(0).parameters().get("cable"); }
    Parameter& pickup() { return chain_.at(0).parameters().get("pickup"); }
    Parameter& boost() { return chain_.at(2).parameters().get("boost"); }

private:
    Chain chain_;
};

} // namespace tonestack::nodes
