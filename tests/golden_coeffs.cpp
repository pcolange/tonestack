// Verifies the engine consumes the compiler-generated coefficient header: BiquadNode driven
// by the generated drive/tone tables reproduces the emitted coefficients and stays stable.
// Built only when the circuitc venv is present (the header is a codegen artifact).

#include <cmath>
#include <vector>

#include "test_framework.h"
#include "tonestack/nodes/BiquadNode.h"
#include "ts9_tables.h"

using namespace tonestack;
namespace gen = tonestack::nodes::generated::ts9;

TS_TEST(generated_tables_have_expected_shape) {
    TS_CHECK(gen::oversample_factor == 4);
    TS_CHECK(gen::drive_num_axes == 1);
    TS_CHECK(gen::tone_num_axes == 1);
    TS_CHECK(gen::drive_dims[0] == 64);
    TS_CHECK(gen::tone_dims[0] == 64);
    TS_CHECK(gen::driveTable(192000.0) != nullptr); // 48k base x4 oversample
    TS_CHECK(gen::toneTable(48000.0) != nullptr);
    TS_CHECK(gen::driveTable(12345.0) == nullptr);   // unsupported rate
}

TS_TEST(biquad_reproduces_generated_section_at_axis_endpoint) {
    // At proportion 1.0 the bound parameter's value is 1.0 — the last table row. An impulse's
    // first output sample equals that row's b0, proving table lookup + node wiring agree with
    // the generated coefficients.
    const nodes::BiquadCoeffTable* table = gen::driveTable(192000.0);
    TS_CHECK(table != nullptr);

    nodes::BiquadNode node(gen::driveParam(), *table);
    node.parameters().get("drive").setProportion(1.0f);
    node.prepare(ProcessSpec{192000.0, 4, 1});

    std::vector<float> x = {1.0f, 0.0f, 0.0f, 0.0f};
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 4);
    node.process(block);

    TS_CHECK_NEAR(x[0], gen::drive_sections_192000[gen::drive_dims[0] - 1].b0, 1e-4);
}

TS_TEST(drive_skew_midpoint_pins_engine_and_compiler_skew_math) {
    // Proportion 0.5 on the log-skewed (A-taper) drive pot must land on the physical skew
    // midpoint (0.1). An impulse's first output then equals the table linearly interpolated
    // at axis value 0.1 — pinning the engine's skew math to the compiler's axis sampling.
    const nodes::BiquadCoeffTable* table = gen::driveTable(192000.0);
    TS_CHECK(table != nullptr);

    nodes::BiquadNode node(gen::driveParam(), *table);
    node.parameters().get("drive").setProportion(0.5f);
    node.prepare(ProcessSpec{192000.0, 4, 1});

    std::vector<float> x = {1.0f, 0.0f, 0.0f, 0.0f};
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 4);
    node.process(block);

    const float v = 0.1f; // driveParam() skew midpoint
    int hi = 1;
    while (hi < gen::drive_dims[0] - 1 && gen::drive_axis0[hi] <= v)
        ++hi;
    const int lo = hi - 1;
    const float t = (v - gen::drive_axis0[lo]) / (gen::drive_axis0[hi] - gen::drive_axis0[lo]);
    const float expectedB0 =
        gen::drive_sections_192000[lo].b0
        + t * (gen::drive_sections_192000[hi].b0 - gen::drive_sections_192000[lo].b0);
    TS_CHECK_NEAR(x[0], expectedB0, 1e-4);
}

TS_TEST(generated_tone_table_is_stable_over_a_block) {
    const nodes::BiquadCoeffTable* table = gen::toneTable(48000.0);
    TS_CHECK(table != nullptr);

    nodes::BiquadNode node(gen::toneParam(), *table);
    node.parameters().get("tone").setProportion(0.7f);
    node.prepare(ProcessSpec{48000.0, 256, 1});

    std::vector<float> x(256);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = std::sin(0.05f * static_cast<float>(i));
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 256);
    node.process(block);

    for (float s : x)
        TS_CHECK(std::isfinite(s));
}
