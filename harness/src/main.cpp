// ToneStack dev harness.
//
// Phase 0: builds a Chain, runs a synthetic signal through it, and reports level so the
// engine can be exercised with no audio device or file I/O. Offline WAV processing and a
// live audio device (miniaudio) arrive with the vendored-dependency pass.

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "tonestack/Chain.h"
#include "tonestack/nodes/GainNode.h"

namespace {

float rms(const std::vector<float>& v) {
    double acc = 0.0;
    for (float s : v) acc += static_cast<double>(s) * s;
    return static_cast<float>(std::sqrt(acc / static_cast<double>(v.empty() ? 1 : v.size())));
}

} // namespace

int main() {
    using namespace tonestack;

    constexpr double sampleRate = 48000.0;
    constexpr int    blockSize = 512;
    constexpr int    numChannels = 2;

    Chain chain;
    auto gain = std::make_unique<nodes::GainNode>();
    gain->parameters().get("gain").setProportion(0.25f); // -> linear gain 0.25
    chain.add(std::move(gain));

    chain.prepare(ProcessSpec{sampleRate, blockSize, numChannels});

    // One block of a 220 Hz sine at amplitude 1.0 on both channels.
    std::vector<float> left(static_cast<size_t>(blockSize));
    std::vector<float> right(static_cast<size_t>(blockSize));
    for (size_t i = 0; i < left.size(); ++i) {
        const float s = std::sin(2.0f * 3.14159265358979323846f * 220.0f
                                 * static_cast<float>(i) / static_cast<float>(sampleRate));
        left[i] = s;
        right[i] = s;
    }

    const float inRms = rms(left);

    float* channels[2] = {left.data(), right.data()};
    AudioBlock block(channels, numChannels, blockSize);
    chain.process(block);

    const float outRms = rms(left);

    std::printf("ToneStack harness\n");
    std::printf("  chain nodes : %d\n", chain.size());
    std::printf("  in  RMS     : %.6f\n", static_cast<double>(inRms));
    std::printf("  out RMS     : %.6f\n", static_cast<double>(outRms));
    std::printf("  ratio       : %.6f (expected ~0.25)\n",
                static_cast<double>(outRms / inRms));
    return 0;
}
