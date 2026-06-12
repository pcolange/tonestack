#include "tonestack/Chain.h"

namespace tonestack {

void Chain::add(std::unique_ptr<Node> node) {
    nodes_.push_back(std::move(node));
}

void Chain::prepare(const ProcessSpec& spec) {
    params_.prepare(spec.sampleRate);
    for (auto& n : nodes_)
        n->prepare(spec);
}

void Chain::reset() noexcept {
    params_.reset();
    for (auto& n : nodes_)
        n->reset();
}

void Chain::process(AudioBlock& io) noexcept {
    params_.snapshotBlock(io.numFrames());
    for (auto& n : nodes_)
        n->process(io);
}

int Chain::latencySamples() const noexcept {
    int total = 0;
    for (const auto& n : nodes_)
        total += n->latencySamples();
    return total;
}

int Chain::tailSamples() const noexcept {
    int total = 0;
    for (const auto& n : nodes_)
        total += n->tailSamples();
    return total;
}

} // namespace tonestack
