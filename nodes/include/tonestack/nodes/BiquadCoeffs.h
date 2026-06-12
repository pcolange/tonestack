#pragma once

namespace tonestack::nodes {

// One biquad second-order section, a0 normalized to 1. Member order matches the transposed
// Direct Form II recurrence used by BiquadNode:
//   y  = b0*x + z1
//   z1 = b1*x - a1*y + z2
//   z2 = b2*x - a2*y
struct BiquadSection {
    float b0 = 1.0f;
    float b1 = 0.0f;
    float b2 = 0.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;
};

// Engine-side cap on table dimensionality; sized for fixed audio-thread scratch arrays.
inline constexpr int kMaxTableAxes = 4;

// Non-owning view of a coefficient grid sampled over one or more parameter axes (pot
// fractions, source-model controls). `axes[k]` is sorted ascending with `dims[k]` entries;
// `sections` is the flattened row-major grid (last axis fastest). The data lives in
// constexpr arrays in a compiler-generated header (nodes/generated/); the engine never
// recomputes coefficients, only multilinearly interpolates the grid. A table is valid for a
// single sample rate — the generated header exposes one table per supported rate, and
// BiquadNode::prepare() rejects a mismatched rate. sampleRate == 0 marks a rate-agnostic
// table (skips that check; intended for hand-built test tables).
struct BiquadCoeffTable {
    const float* const*  axes = nullptr;     // [numAxes][dims[k]], each ascending
    const int*           dims = nullptr;     // [numAxes]
    int                  numAxes = 0;        // 1..kMaxTableAxes
    const BiquadSection* sections = nullptr; // [prod(dims)], row-major, last axis fastest
    double               sampleRate = 0.0;
};

} // namespace tonestack::nodes
