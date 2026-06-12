#pragma once

#include <algorithm>

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

} // namespace tonestack::nodes::detail
