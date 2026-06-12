// ToneStack dev harness.
//
// Runs synthetic signals through the engine with no audio device or file I/O: a gain chain
// (the Phase 0 smoke test) and, when the circuitc-generated tables are present, the
// Rangemaster module's signature behaviors (treble boost, guitar-volume cleanup). Offline
// WAV processing and a live audio device (miniaudio) arrive with the vendored-dependency
// pass.

#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

#include "tonestack/Chain.h"
#include "tonestack/nodes/GainNode.h"

#if TONESTACK_HAVE_GENERATED
#include "tonestack/nodes/RangemasterModule.h"
#endif

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlockSize = 512;

float rms(const std::vector<float>& v, size_t from = 0) {
    double acc = 0.0;
    for (size_t i = from; i < v.size(); ++i)
        acc += static_cast<double>(v[i]) * v[i];
    const size_t n = v.size() - from;
    return static_cast<float>(std::sqrt(acc / static_cast<double>(n != 0 ? n : 1)));
}

std::vector<float> sine(float freq, float amp, int frames) {
    std::vector<float> v(static_cast<size_t>(frames));
    for (int i = 0; i < frames; ++i)
        v[static_cast<size_t>(i)] =
            amp * std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) /
                           static_cast<float>(kSampleRate));
    return v;
}

void processBlocks(tonestack::Node& node, std::vector<float>& buf) {
    for (size_t start = 0; start < buf.size(); start += kBlockSize) {
        float* chans[1] = {buf.data() + start};
        tonestack::AudioBlock block(chans, 1, kBlockSize);
        node.process(block);
    }
}

void runGainDemo() {
    using namespace tonestack;

    Chain chain;
    auto gain = std::make_unique<nodes::GainNode>();
    gain->parameters().get("gain").setProportion(0.25f); // -> linear gain 0.25
    chain.add(std::move(gain));
    chain.prepare(ProcessSpec{kSampleRate, kBlockSize, 1});

    std::vector<float> buf = sine(220.0f, 1.0f, 4096);
    const float inRms = rms(buf);
    processBlocks(chain, buf);

    std::printf("gain chain   : in RMS %.4f -> out RMS %.4f (ratio %.4f, expected ~0.25)\n",
                static_cast<double>(inRms), static_cast<double>(rms(buf)),
                static_cast<double>(rms(buf) / inRms));
}

#if TONESTACK_HAVE_GENERATED
// Linear-regime module gain at `freq` for a given guitar-volume proportion.
float rangemasterGain(float freq, float volumeProportion) {
    using namespace tonestack;

    nodes::RangemasterModule module(kSampleRate);
    module.volume().setProportion(volumeProportion);
    module.boost().setProportion(1.0f);
    module.prepare(ProcessSpec{kSampleRate, kBlockSize, 1});

    const float amp = 0.005f; // ~5 mV: below the germanium knee
    std::vector<float> buf = sine(freq, amp, 8192);
    processBlocks(module, buf);
    return rms(buf, buf.size() / 2) / (amp / std::sqrt(2.0f));
}

void runRangemasterDemo() {
    using namespace tonestack;
    nodes::RangemasterModule probe(kSampleRate);
    probe.prepare(ProcessSpec{kSampleRate, kBlockSize, 1});
    std::printf("rangemaster  : latency %d samples @ 48k\n", probe.latencySamples());
    const float low = rangemasterGain(150.0f, 1.0f);
    const float mid = rangemasterGain(2000.0f, 1.0f);
    const float midHalfVol = rangemasterGain(2000.0f, 0.5f);
    std::printf("  treble boost : %.1f dB @ 2 kHz vs %.1f dB @ 150 Hz (full volume)\n",
                20.0 * std::log10(static_cast<double>(mid)),
                20.0 * std::log10(static_cast<double>(low)));
    std::printf("  vol cleanup  : %.1f dB drop @ 2 kHz at half guitar volume\n",
                20.0 * std::log10(static_cast<double>(mid / midHalfVol)));
}
#endif

} // namespace

int main() {
    std::printf("ToneStack harness (%.0f Hz, block %d)\n", kSampleRate, kBlockSize);
    runGainDemo();
#if TONESTACK_HAVE_GENERATED
    runRangemasterDemo();
#else
    std::printf("rangemaster  : skipped (no circuitc venv / generated tables)\n");
#endif
    return 0;
}
