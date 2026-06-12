#pragma once

#include <memory>
#include <vector>

#include "tonestack/Node.h"

namespace tonestack {

// A linear series of nodes processed in order, in place. Because a Chain is itself a
// Node, composite modules (e.g. a Tube Screamer) are built as a Chain and then used as
// a single node inside a larger graph.
//
// Build the chain (add()) before prepare(); the node list is never mutated on the audio
// thread. A DAG/Graph generalization comes later — a linear chain covers most guitar
// signal paths.
class Chain : public Node {
public:
    NodeInfo info() const noexcept override { return {"chain"}; }

    // setup thread only
    void add(std::unique_ptr<Node> node);
    int  size() const noexcept { return static_cast<int>(nodes_.size()); }
    Node& at(int i) noexcept { return *nodes_[static_cast<size_t>(i)]; }

    void prepare(const ProcessSpec& spec) override;
    void reset() noexcept override;
    void process(AudioBlock& io) noexcept override;

    // Chain owns no parameters of its own yet; per-node parameter flattening
    // (namespaced host binding) is a later phase.
    ParameterSet& parameters() noexcept override { return params_; }

    int latencySamples() const noexcept override;
    int tailSamples() const noexcept override;

private:
    std::vector<std::unique_ptr<Node>> nodes_;
    ParameterSet params_;
};

} // namespace tonestack
