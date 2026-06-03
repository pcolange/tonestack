#pragma once

namespace tonestack {

// Non-owning view over caller-owned, deinterleaved audio. This is ToneStack's own
// type so the engine never depends on a plugin SDK's buffer class.
class AudioBlock {
public:
    AudioBlock() = default;

    AudioBlock(float* const* channels, int numChannels, int numFrames) noexcept
        : channels_(channels), numChannels_(numChannels), numFrames_(numFrames) {}

    int numChannels() const noexcept { return numChannels_; }
    int numFrames()   const noexcept { return numFrames_; }

    float*       channel(int c)       noexcept { return channels_[c]; }
    const float* channel(int c) const noexcept { return channels_[c]; }

private:
    float* const* channels_ = nullptr;
    int           numChannels_ = 0;
    int           numFrames_ = 0;
};

} // namespace tonestack
