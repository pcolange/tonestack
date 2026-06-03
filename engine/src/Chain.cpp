#include "tonestack/Chain.h"

namespace tonestack {

void Chain::add(std::unique_ptr<Node> node) {
    nodes_.push_back(std::move(node));
}

void Chain::prepare(const ProcessSpec& spec) {
    for (auto& n : nodes_)
        n->prepare(spec);
}

void Chain::reset() noexcept {
    for (auto& n : nodes_)
        n->reset();
}

void Chain::process(AudioBlock& io) noexcept {
    for (auto& n : nodes_)
        n->process(io);
}

int Chain::latencySamples() const noexcept {
    int total = 0;
    for (const auto& n : nodes_)
        total += n->latencySamples();
    return total;
}

} // namespace tonestack
