#include <array>
#include <stdexcept>
#include <vector>

#include "test_framework.h"
#include "tonestack/nodes/BiquadCoeffs.h"
#include "tonestack/nodes/BiquadNode.h"

using namespace tonestack;
using nodes::BiquadCoeffTable;
using nodes::BiquadNode;
using nodes::BiquadSection;

namespace {

ParameterDesc linearPot(const char* id) {
    return ParameterDesc{id, 0.0f, 1.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f};
}

// Reference transposed Direct Form II, the recurrence BiquadNode must reproduce.
std::vector<float> referenceTdf2(const BiquadSection& c, const std::vector<float>& x) {
    std::vector<float> y(x.size());
    float z1 = 0.0f, z2 = 0.0f;
    for (size_t i = 0; i < x.size(); ++i) {
        const float out = c.b0 * x[i] + z1;
        z1 = c.b1 * x[i] - c.a1 * out + z2;
        z2 = c.b2 * x[i] - c.a2 * out;
        y[i] = out;
    }
    return y;
}

} // namespace

TS_TEST(biquad_matches_reference_recurrence) {
    const BiquadSection c{0.2f, 0.1f, 0.05f, -0.3f, 0.1f}; // arbitrary stable section
    static const float axis0[1] = {0.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {1};
    static const BiquadSection sections[1] = {c};
    BiquadCoeffTable table{axes, dims, 1, sections, 48000.0};

    BiquadNode node(linearPot("drive"), table);
    node.prepare(ProcessSpec{48000.0, 16, 1});

    std::vector<float> x(16);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = std::sin(0.7f * static_cast<float>(i)) + 0.25f * static_cast<float>(i % 3);
    const std::vector<float> expected = referenceTdf2(c, x);

    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 16);
    node.process(block);

    for (size_t i = 0; i < x.size(); ++i)
        TS_CHECK_NEAR(x[i], expected[i], 1e-6);
}

TS_TEST(biquad_interpolates_between_rows) {
    // Two rows sharing poles; b coefficients scale 1x -> 2x so the section is linear in the
    // table position. At axis 0.5 the interpolated b0 must be the midpoint (0.15).
    static const float axis0[2] = {0.0f, 1.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {2};
    static const BiquadSection sections[2] = {
        {0.1f, 0.0f, 0.0f, -0.3f, 0.1f},
        {0.2f, 0.0f, 0.0f, -0.3f, 0.1f},
    };
    BiquadCoeffTable table{axes, dims, 1, sections, 48000.0};

    BiquadNode node(linearPot("tone"), table);
    node.parameters().get("tone").setProportion(0.5f); // linear -> axis value 0.5
    node.prepare(ProcessSpec{48000.0, 4, 1});

    std::vector<float> x = {1.0f, 0.0f, 0.0f, 0.0f}; // impulse: first output sample == b0
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 4);
    node.process(block);

    TS_CHECK_NEAR(x[0], 0.15, 1e-6);
}

TS_TEST(biquad_bilinear_interpolation_over_two_axes) {
    // 2x2 grid whose b0 is bilinear by construction: b0(x, y) = 0.1 + 0.2x + 0.3y + 0.4xy.
    // At (x, y) = (0.25, 0.75) the blend must give exactly 0.45.
    static const float axisX[2] = {0.0f, 1.0f};
    static const float axisY[2] = {0.0f, 1.0f};
    static const float* axes[2] = {axisX, axisY};
    static const int dims[2] = {2, 2};
    // Row-major, last axis (y) fastest: (x0,y0), (x0,y1), (x1,y0), (x1,y1).
    static const BiquadSection sections[4] = {
        {0.1f, 0.0f, 0.0f, -0.3f, 0.1f},
        {0.4f, 0.0f, 0.0f, -0.3f, 0.1f},
        {0.3f, 0.0f, 0.0f, -0.3f, 0.1f},
        {1.0f, 0.0f, 0.0f, -0.3f, 0.1f},
    };
    BiquadCoeffTable table{axes, dims, 2, sections, 48000.0};

    BiquadNode node({linearPot("x"), linearPot("y")}, table);
    node.parameters().get("x").setProportion(0.25f);
    node.parameters().get("y").setProportion(0.75f);
    node.prepare(ProcessSpec{48000.0, 4, 1});

    std::vector<float> x = {1.0f, 0.0f, 0.0f, 0.0f};
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 4);
    node.process(block);

    TS_CHECK_NEAR(x[0], 0.45, 1e-6);
}

TS_TEST(biquad_rejects_mismatched_table_rate) {
    static const float axis0[1] = {0.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {1};
    static const BiquadSection sections[1] = {{0.5f, 0.0f, 0.0f, -0.5f, 0.0f}};
    BiquadCoeffTable table{axes, dims, 1, sections, 44100.0};

    BiquadNode node(linearPot("drive"), table);
    bool threw = false;
    try {
        node.prepare(ProcessSpec{48000.0, 16, 1});
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TS_CHECK(threw);
}

TS_TEST(biquad_rejects_param_axis_count_mismatch) {
    static const float axis0[1] = {0.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {1};
    static const BiquadSection sections[1] = {{0.5f, 0.0f, 0.0f, -0.5f, 0.0f}};
    BiquadCoeffTable table{axes, dims, 1, sections, 48000.0};

    bool threw = false;
    try {
        BiquadNode node({linearPot("a"), linearPot("b")}, table); // 2 params, 1 axis
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TS_CHECK(threw);
}

TS_TEST(biquad_channel_state_is_independent) {
    static const float axis0[1] = {0.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {1};
    static const BiquadSection sections[1] = {{0.5f, 0.0f, 0.0f, -0.5f, 0.0f}};
    BiquadCoeffTable table{axes, dims, 1, sections, 48000.0};

    BiquadNode node(linearPot("drive"), table);
    node.prepare(ProcessSpec{48000.0, 8, 2});

    std::vector<float> l(8, 1.0f), r(8, 0.0f);
    float* chans[2] = {l.data(), r.data()};
    AudioBlock block(chans, 2, 8);
    node.process(block);

    // Right channel fed silence stays silent; left does not. Proves separate z-state.
    for (float s : r) TS_CHECK_NEAR(s, 0.0, 1e-9);
    TS_CHECK(std::fabs(l[0]) > 0.0f);
}
