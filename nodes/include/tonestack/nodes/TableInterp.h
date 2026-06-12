#pragma once

#include <algorithm>

#include "tonestack/nodes/BiquadCoeffs.h" // kMaxTableAxes

namespace tonestack::nodes::detail {

// Bracket `v` on an ascending axis: lower grid index and interpolation fraction, clamped.
// Shared by the table-driven nodes (BiquadNode, IirNode).
inline void bracketAxis(const float* axis, int dim, float v, int& i0, float& t) noexcept {
    if (dim <= 1) {
        i0 = 0;
        t = 0.0f;
        return;
    }
    const float* hi = std::upper_bound(axis, axis + dim, v);
    if (hi == axis) {
        i0 = 0;
        t = 0.0f;
        return;
    }
    if (hi == axis + dim) {
        i0 = dim - 2;
        t = 1.0f;
        return;
    }
    i0 = static_cast<int>(hi - axis) - 1;
    const float a0 = axis[i0];
    const float a1 = axis[i0 + 1];
    const float span = a1 - a0;
    t = span > 0.0f ? (v - a0) / span : 0.0f;
}

// The 2^N bracketing corners of a row-major coefficient grid (last axis fastest) at one
// lookup point: flat row indices and multilinear weights, zero-weight corners dropped.
// The caller accumulates its own row representation from them.
struct GridCorners {
    int   count = 0;
    int   index[1 << kMaxTableAxes];
    float weight[1 << kMaxTableAxes];
};

inline void gatherCorners(const float* const* axes, const int* dims, int numAxes,
                          const float* values, GridCorners& out) noexcept {
    int   i0[kMaxTableAxes];
    float t[kMaxTableAxes];
    int   stride[kMaxTableAxes];
    for (int k = 0; k < numAxes; ++k)
        bracketAxis(axes[k], dims[k], values[k], i0[k], t[k]);
    stride[numAxes - 1] = 1;
    for (int k = numAxes - 2; k >= 0; --k)
        stride[k] = stride[k + 1] * dims[k + 1];

    out.count = 0;
    const int corners = 1 << numAxes;
    for (int c = 0; c < corners; ++c) {
        float w = 1.0f;
        int idx = 0;
        for (int k = 0; k < numAxes; ++k) {
            const int bit = (c >> k) & 1;
            w *= bit != 0 ? t[k] : 1.0f - t[k];
            idx += stride[k] * std::min(i0[k] + bit, dims[k] - 1);
        }
        if (w == 0.0f)
            continue;
        out.index[out.count] = idx;
        out.weight[out.count] = w;
        ++out.count;
    }
}

} // namespace tonestack::nodes::detail
