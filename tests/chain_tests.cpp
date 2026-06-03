#include <memory>
#include <vector>

#include "test_framework.h"
#include "tonestack/Chain.h"
#include "tonestack/nodes/GainNode.h"

using namespace tonestack;

TS_TEST(gain_node_applies_gain) {
    Chain chain;
    auto g = std::make_unique<nodes::GainNode>();
    g->parameters().get("gain").setProportion(0.25f);
    chain.add(std::move(g));
    chain.prepare(ProcessSpec{48000.0, 8, 1});

    std::vector<float> buf(8, 1.0f);
    float* chans[1] = {buf.data()};
    AudioBlock block(chans, 1, 8);
    chain.process(block);

    for (float s : buf)
        TS_CHECK_NEAR(s, 0.25, 1e-6);
}

TS_TEST(chain_runs_nodes_in_order) {
    Chain chain;
    auto a = std::make_unique<nodes::GainNode>();
    a->parameters().get("gain").setProportion(0.5f);
    auto b = std::make_unique<nodes::GainNode>();
    b->parameters().get("gain").setProportion(0.5f);
    chain.add(std::move(a));
    chain.add(std::move(b));
    chain.prepare(ProcessSpec{48000.0, 4, 1});

    TS_CHECK(chain.size() == 2);

    std::vector<float> buf(4, 1.0f);
    float* chans[1] = {buf.data()};
    AudioBlock block(chans, 1, 4);
    chain.process(block);

    for (float s : buf)
        TS_CHECK_NEAR(s, 0.25, 1e-6); // 0.5 * 0.5
}

TS_TEST(gain_node_is_stereo_agnostic) {
    Chain chain;
    auto g = std::make_unique<nodes::GainNode>();
    g->parameters().get("gain").setProportion(0.5f);
    chain.add(std::move(g));
    chain.prepare(ProcessSpec{48000.0, 4, 2});

    std::vector<float> l(4, 1.0f), r(4, 2.0f);
    float* chans[2] = {l.data(), r.data()};
    AudioBlock block(chans, 2, 4);
    chain.process(block);

    for (float s : l) TS_CHECK_NEAR(s, 0.5, 1e-6);
    for (float s : r) TS_CHECK_NEAR(s, 1.0, 1e-6); // each channel processed independently
}
