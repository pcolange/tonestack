// Verifies the engine consumes the Rangemaster generated header: IirNode driven by the
// generated input grid reproduces the emitted coefficients, and the composite module shows
// the pedal's signature behaviors end to end. Built only when the circuitc venv is present.

#include <cmath>
#include <memory>
#include <vector>

#include "rangemaster_tables.h"
#include "test_framework.h"
#include "tonestack/nodes/IirNode.h"
#include "tonestack/nodes/RangemasterModule.h"

using namespace tonestack;
namespace gen = tonestack::nodes::generated::rangemaster;

namespace {

// RMS gain of the module for a small (linear-regime) sine at `freq`.
float moduleGainAt(float freq, double fs) {
    nodes::RangemasterModule module(fs);
    module.volume().setProportion(1.0f);
    module.boost().setProportion(1.0f);
    module.prepare(ProcessSpec{fs, 512, 1});

    const int total = 4096;
    const float amp = 0.005f; // ~5 mV: well below the germanium knee
    std::vector<float> x(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i)
        x[static_cast<size_t>(i)] =
            amp * std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) /
                           static_cast<float>(fs));

    for (int start = 0; start < total; start += 512) {
        float* chans[1] = {x.data() + start};
        AudioBlock block(chans, 1, 512);
        module.process(block);
    }

    double acc = 0.0;
    for (int i = total / 2; i < total; ++i) // steady state
        acc += static_cast<double>(x[static_cast<size_t>(i)]) *
               static_cast<double>(x[static_cast<size_t>(i)]);
    const double rms = std::sqrt(acc / (total / 2.0));
    return static_cast<float>(rms / (amp / std::sqrt(2.0)));
}

} // namespace

TS_TEST(rangemaster_tables_have_expected_shape) {
    TS_CHECK(gen::input_order == 4);
    TS_CHECK(gen::input_num_axes == 3);
    TS_CHECK(gen::input_dims[0] == 25);
    TS_CHECK(gen::input_dims[1] == 5);
    TS_CHECK(gen::input_dims[2] == 5);
    TS_CHECK(gen::output_num_axes == 1);
    TS_CHECK(gen::inputTable(48000.0) != nullptr);
    TS_CHECK(gen::outputTable(96000.0) != nullptr);
    TS_CHECK(gen::inputTable(12345.0) == nullptr);
}

TS_TEST(iir_node_reproduces_generated_corner_row) {
    // All knobs at proportion 1.0 -> the last grid row exactly; an impulse's first output
    // sample equals that row's b0, pinning grid lookup + node wiring to the emitted data.
    const nodes::IirCoeffTable* table = gen::inputTable(48000.0);
    TS_CHECK(table != nullptr);

    nodes::IirNode node({gen::inputParam(0), gen::inputParam(1), gen::inputParam(2)},
                        *table);
    node.parameters().get("volume").setProportion(1.0f);
    node.parameters().get("cable").setProportion(1.0f);
    node.parameters().get("pickup").setProportion(1.0f);
    node.prepare(ProcessSpec{48000.0, 4, 1});

    std::vector<float> x = {1.0f, 0.0f, 0.0f, 0.0f};
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 4);
    node.process(block);

    const int total = gen::input_dims[0] * gen::input_dims[1] * gen::input_dims[2];
    const int width = 2 * gen::input_order + 1;
    const float expectedB0 = gen::input_coeffs_48000[(total - 1) * width];
    TS_CHECK_NEAR(x[0], expectedB0, 1e-5);
}

TS_TEST(rangemaster_module_boosts_treble) {
    const float low = moduleGainAt(150.0f, 48000.0);
    const float mid = moduleGainAt(2000.0f, 48000.0);
    TS_CHECK(mid > 5.0f);       // strong boost in the presence region
    TS_CHECK(mid / low > 4.0f); // and it is a *treble* booster
}

TS_TEST(rangemaster_module_cleans_up_with_guitar_volume) {
    nodes::RangemasterModule full(48000.0);
    nodes::RangemasterModule half(48000.0);
    full.volume().setProportion(1.0f);
    half.volume().setProportion(0.5f);
    full.boost().setProportion(1.0f);
    half.boost().setProportion(1.0f);
    full.prepare(ProcessSpec{48000.0, 512, 1});
    half.prepare(ProcessSpec{48000.0, 512, 1});

    const int total = 2048;
    std::vector<float> a(static_cast<size_t>(total)), b(static_cast<size_t>(total));
    for (int i = 0; i < total; ++i) {
        const float s = 0.005f * std::sin(2.0f * 3.14159265f * 3000.0f *
                                          static_cast<float>(i) / 48000.0f);
        a[static_cast<size_t>(i)] = s;
        b[static_cast<size_t>(i)] = s;
    }
    for (int start = 0; start < total; start += 512) {
        float* ca[1] = {a.data() + start};
        float* cb[1] = {b.data() + start};
        AudioBlock ba(ca, 1, 512), bb(cb, 1, 512);
        full.process(ba);
        half.process(bb);
    }
    double ra = 0.0, rb = 0.0;
    for (int i = total / 2; i < total; ++i) {
        ra += static_cast<double>(a[static_cast<size_t>(i)]) * a[static_cast<size_t>(i)];
        rb += static_cast<double>(b[static_cast<size_t>(i)]) * b[static_cast<size_t>(i)];
    }
    TS_CHECK(ra > 4.0 * rb); // > 6 dB level drop at half volume
}

TS_TEST(rangemaster_module_reports_oversampler_latency) {
    nodes::RangemasterModule module(48000.0);
    module.prepare(ProcessSpec{48000.0, 256, 1});
    TS_CHECK(module.latencySamples() == 29);
}
