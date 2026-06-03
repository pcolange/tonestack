#include "test_framework.h"
#include "tonestack/Parameter.h"

using namespace tonestack;

TS_TEST(parameter_linear_range) {
    Parameter p(ParameterDesc{"x", 0.0f, 10.0f, 0.5f, ParamSkew::Linear, 0.5f, 0.0f});
    p.prepare(48000.0, 512);
    p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 5.0, 1e-6); // proportion 0.5 over [0,10]
}

TS_TEST(parameter_log_skew_hits_midpoint) {
    // Log skew: value == skewMidpoint at proportion 0.5.
    Parameter p(ParameterDesc{"tone", 0.0f, 1.0f, 0.5f, ParamSkew::Logarithmic, 0.8f, 0.0f});
    p.prepare(48000.0, 512);

    p.setProportion(0.5f); p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 0.8, 1e-6);

    p.setProportion(1.0f); p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 1.0, 1e-6);

    p.setProportion(0.0f); p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 0.0, 1e-6);
}

TS_TEST(parameter_clamps_proportion) {
    Parameter p(ParameterDesc{"x", 0.0f, 1.0f, 0.0f, ParamSkew::Linear, 0.5f, 0.0f});
    p.prepare(48000.0, 512);

    p.setProportion(2.0f);  p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 1.0, 1e-6);

    p.setProportion(-1.0f); p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 0.0, 1e-6);
}

TS_TEST(parameter_smoothing_converges) {
    Parameter p(ParameterDesc{"x", 0.0f, 1.0f, 0.0f, ParamSkew::Linear, 0.5f, 0.02f});
    p.prepare(48000.0, 64);
    p.setProportion(1.0f);
    for (int i = 0; i < 2000; ++i)
        p.snapshotBlock();
    TS_CHECK_NEAR(p.value(), 1.0, 1e-3); // smoothed value reaches target
}
