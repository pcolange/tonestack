#include <cmath>
#include <stdexcept>
#include <vector>

#include "test_framework.h"
#include "tonestack/nodes/IirCoeffs.h"
#include "tonestack/nodes/IirNode.h"

using namespace tonestack;
using nodes::IirCoeffTable;
using nodes::IirNode;

namespace {

ParameterDesc linearPot(const char* id) {
    return ParameterDesc{id, 0.0f, 1.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f};
}

// Reference transposed Direct Form II of arbitrary order.
std::vector<float> referenceTdf2(const float* row, int order, const std::vector<float>& x) {
    const float* b = row;
    const float* a = row + order + 1;
    std::vector<float> z(static_cast<size_t>(order), 0.0f);
    std::vector<float> y(x.size());
    for (size_t i = 0; i < x.size(); ++i) {
        const float out = b[0] * x[i] + z[0];
        for (int k = 0; k < order - 1; ++k)
            z[static_cast<size_t>(k)] =
                b[k + 1] * x[i] - a[k] * out + z[static_cast<size_t>(k + 1)];
        z[static_cast<size_t>(order - 1)] = b[order] * x[i] - a[order - 1] * out;
        y[i] = out;
    }
    return y;
}

} // namespace

TS_TEST(iir_matches_reference_recurrence) {
    // Arbitrary stable 4th-order row: b0..b4, a1..a4.
    static const float row[9] = {0.2f,  0.1f, -0.05f, 0.02f, 0.01f,
                                 -0.8f, 0.3f, -0.1f,  0.02f};
    static const float axis0[1] = {0.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {1};
    IirCoeffTable table{axes, dims, 1, row, 4, 48000.0};

    IirNode node(linearPot("volume"), table);
    node.prepare(ProcessSpec{48000.0, 16, 1});

    std::vector<float> x(16);
    for (size_t i = 0; i < x.size(); ++i)
        x[i] = std::sin(0.6f * static_cast<float>(i)) + 0.2f * static_cast<float>(i % 4);
    const std::vector<float> expected = referenceTdf2(row, 4, x);

    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 16);
    node.process(block);

    for (size_t i = 0; i < x.size(); ++i)
        TS_CHECK_NEAR(x[i], expected[i], 1e-6);
}

TS_TEST(iir_interpolates_rows) {
    // Two grid rows whose b0 differs (0.1 vs 0.3); at axis value 0.5 the blended b0 must
    // be 0.2 — an impulse's first output sample exposes it.
    static const float rows[2 * 5] = {
        0.1f, 0.0f, 0.0f, -0.3f, 0.1f, // row 0: b0,b1,b2,a1,a2 (order 2)
        0.3f, 0.0f, 0.0f, -0.3f, 0.1f,
    };
    static const float axis0[2] = {0.0f, 1.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {2};
    IirCoeffTable table{axes, dims, 1, rows, 2, 48000.0};

    IirNode node(linearPot("volume"), table);
    node.parameters().get("volume").setProportion(0.5f);
    node.prepare(ProcessSpec{48000.0, 4, 1});

    std::vector<float> x = {1.0f, 0.0f, 0.0f, 0.0f};
    float* chans[1] = {x.data()};
    AudioBlock block(chans, 1, 4);
    node.process(block);

    TS_CHECK_NEAR(x[0], 0.2, 1e-6);
}

TS_TEST(iir_rejects_bad_contracts) {
    static const float row[5] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    static const float axis0[1] = {0.0f};
    static const float* axes[1] = {axis0};
    static const int dims[1] = {1};

    bool threw = false;
    try {
        IirCoeffTable table{axes, dims, 1, row, 2, 44100.0};
        IirNode node(linearPot("v"), table);
        node.prepare(ProcessSpec{48000.0, 16, 1}); // rate mismatch
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TS_CHECK(threw);

    threw = false;
    try {
        IirCoeffTable table{axes, dims, 1, row, 0, 48000.0}; // order out of range
        IirNode node(linearPot("v"), table);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TS_CHECK(threw);

    threw = false;
    try {
        IirCoeffTable table{axes, dims, 1, row, 2, 48000.0};
        IirNode node({linearPot("a"), linearPot("b")}, table); // 2 params, 1 axis
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    TS_CHECK(threw);
}
