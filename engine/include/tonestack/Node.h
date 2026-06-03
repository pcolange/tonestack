#pragma once

#include "tonestack/AudioBlock.h"
#include "tonestack/NodeInfo.h"
#include "tonestack/ParameterSet.h"
#include "tonestack/ProcessSpec.h"

namespace tonestack {

// The core processing abstraction. Everything in the graph — a biquad, a waveshaper,
// a neural block, a whole composite module — is a Node.
//
// Threading / real-time contract:
//   - prepare()  : the ONLY place a node may allocate. Setup thread.
//   - process()  : real-time safe. No allocation, no locks, no exceptions.
//   - reset()    : clears state without allocating. Real-time safe.
class Node {
public:
    virtual ~Node() = default;

    virtual NodeInfo info() const noexcept = 0;

    virtual void prepare(const ProcessSpec& spec) = 0;
    virtual void reset() noexcept = 0;
    virtual void process(AudioBlock& io) noexcept = 0;

    virtual ParameterSet& parameters() noexcept = 0;

    virtual int latencySamples() const noexcept { return 0; }
    virtual int tailSamples() const noexcept { return 0; }
};

} // namespace tonestack
