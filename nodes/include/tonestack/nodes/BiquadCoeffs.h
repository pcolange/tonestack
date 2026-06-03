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

// Non-owning view of a coefficient table sampled across a parameter axis (e.g. a pot
// fraction). `axis` is sorted ascending; row i holds the discretized section at axis[i].
// The data lives in constexpr arrays in a compiler-generated header (nodes/generated/); the
// engine never recomputes coefficients, only interpolates between adjacent rows. A table is
// valid for a single sample rate — the generated header exposes one table per supported rate.
struct BiquadCoeffTable {
    const float*         axis = nullptr;     // [rows], ascending parameter values
    const BiquadSection* sections = nullptr; // [rows]
    int                  rows = 0;
    double               sampleRate = 0.0;
};

} // namespace tonestack::nodes
