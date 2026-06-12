#include <cmath>
#include <memory>
#include <vector>

#include "test_framework.h"
#include "tonestack/nodes/GainNode.h"
#include "tonestack/nodes/OversampledNode.h"
#include "tonestack/nodes/WaveshaperNode.h"

using namespace tonestack;
using nodes::AsinhDiodeShape;
using nodes::OversampledNode;
using nodes::WaveshaperNode;

namespace {

ParameterDesc drivePot() {
    return ParameterDesc{"drive", 0.0f, 1.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f};
}

} // namespace

TS_TEST(waveshaper_applies_shape) {
    const AsinhDiodeShape shape{2.52e-9f, 45.3e-3f, 51e3f, 500e3f};
    WaveshaperNode<AsinhDiodeShape> node(drivePot(), shape);
    node.parameters().get("drive").setProportion(1.0f);
    node.prepare(ProcessSpec{48000.0, 8, 1});

    std::vector<float> x = {0.5f, -0.8f, 1.5f, -2.0f, 0.9f, -0.4f, 1.1f, 0.0f};
    const std::vector<float> in = x;
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 8);
    node.process(block);

    for (size_t i = 0; i < x.size(); ++i) {
        const float expected = shape(in[i], 1.0f);
        TS_CHECK_NEAR(x[i], expected, 1e-6);
    }
}

TS_TEST(waveshaper_clamps_to_input_for_small_signals) {
    // The asinh slope at the origin far exceeds 1, so small inputs hit the |U| <= |x| clamp
    // and pass through unchanged (diodes below conduction add nothing).
    const AsinhDiodeShape shape{2.52e-9f, 45.3e-3f, 51e3f, 500e3f};
    TS_CHECK_NEAR(shape(1e-6f, 0.5f), 1e-6f, 1e-12);
    TS_CHECK_NEAR(shape(-1e-6f, 0.5f), -1e-6f, 1e-12);
}

TS_TEST(oversampled_unity_inner_nulls_against_delayed_input) {
    auto gain = std::make_unique<nodes::GainNode>();
    gain->parameters().get("gain").setProportion(1.0f); // unity
    OversampledNode node(std::move(gain));
    node.prepare(ProcessSpec{48000.0, 512, 1});

    const size_t total = 4096;
    std::vector<float> in(total), out(total);
    for (size_t i = 0; i < total; ++i)
        in[i] = std::sin(2.0f * 3.14159265f * 1000.0f * static_cast<float>(i) / 48000.0f);
    out = in;

    for (size_t start = 0; start < total; start += 512) {
        float* chans[1] = {out.data() + start};
        AudioBlock block(chans, 1, 512);
        node.process(block);
    }

    const size_t latency = static_cast<size_t>(node.latencySamples());
    double errAcc = 0.0, sigAcc = 0.0;
    for (size_t i = 256; i < total; ++i) {
        const double e = static_cast<double>(out[i]) - static_cast<double>(in[i - latency]);
        errAcc += e * e;
        sigAcc += static_cast<double>(in[i - latency]) * static_cast<double>(in[i - latency]);
    }
    const double errRms = std::sqrt(errAcc / sigAcc);
    TS_CHECK(errRms < 1e-3); // ~ -60 dB null through up/down resampling
}

TS_TEST(oversampled_reports_exact_fir_latency) {
    auto gain = std::make_unique<nodes::GainNode>();
    gain->parameters().get("gain").setProportion(1.0f);
    OversampledNode node(std::move(gain));
    node.prepare(ProcessSpec{48000.0, 256, 1});

    std::vector<float> x(256, 0.0f);
    x[0] = 1.0f;
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 256);
    node.process(block);

    size_t peak = 0;
    for (size_t i = 1; i < x.size(); ++i)
        if (std::fabs(x[i]) > std::fabs(x[peak]))
            peak = i;
    TS_CHECK(node.latencySamples() == 29);
    TS_CHECK(peak == static_cast<size_t>(node.latencySamples()));
}

TS_TEST(oversampled_forwards_inner_parameters) {
    auto gain = std::make_unique<nodes::GainNode>();
    OversampledNode node(std::move(gain));
    TS_CHECK(node.parameters().indexOf("gain") == 0);
}
