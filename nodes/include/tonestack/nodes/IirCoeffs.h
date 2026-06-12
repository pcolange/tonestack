#pragma once

#include "tonestack/nodes/BiquadCoeffs.h" // kMaxTableAxes

namespace tonestack::nodes {

// Engine-side cap on direct-form order; sized for fixed audio-thread scratch arrays.
inline constexpr int kMaxIirOrder = 8;

// Non-owning view of a direct-form IIR coefficient grid sampled over parameter axes.
// `coeffs` holds prod(dims) rows of `2*order + 1` floats each (`b0..bN, a1..aN`, a0
// normalized to 1), row-major with the last axis fastest. Used for networks whose factored
// per-section form cannot be interpolated continuously across the grid (root assignments
// regroup at complex breakaways); the unfactored coefficients are polynomial in the
// component values and interpolate smoothly. Direct form is only float-safe while poles
// stay well separated — the compiler's grid tests enforce stability, including at blends.
// Same sample-rate contract as BiquadCoeffTable (0 = rate-agnostic test table).
struct IirCoeffTable {
    const float* const* axes = nullptr; // [numAxes][dims[k]], each ascending
    const int*          dims = nullptr; // [numAxes]
    int                 numAxes = 0;    // 1..kMaxTableAxes
    const float*        coeffs = nullptr; // [prod(dims)][2*order+1] flat
    int                 order = 0;        // 1..kMaxIirOrder
    double              sampleRate = 0.0;
};

} // namespace tonestack::nodes
