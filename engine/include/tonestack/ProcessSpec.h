#pragma once

namespace tonestack {

// Audio configuration handed to every Node in prepare(). All allocation a node
// needs must be sized from these values and done in prepare(), never in process().
struct ProcessSpec {
    double sampleRate = 44100.0;
    int    maxBlockSize = 512;
    int    numChannels = 2;
};

} // namespace tonestack
