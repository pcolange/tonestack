#pragma once

#include <cmath>

namespace tonestack::nodes {

// Memoryless shape functions for WaveshaperNode. Each is a POD whose constants live in a
// compiler-generated header (constexpr instance); the node applies it per sample. `control`
// is the bound parameter's physical value (e.g. a pot fraction), constant within a block.

// Diode/op-amp soft clipper split out of the TS9 feedback path: the diode voltage
// U = nVt * asinh(x / (2*Is*R2)) with R2 = r2Const + r2Pot * control, replaced by x itself
// when |U| would exceed |x| (the diodes cannot drop more than the driving signal).
struct AsinhDiodeShape {
    float saturationCurrent = 1.0f; // Is
    float thermalVoltage = 1.0f;    // n*Vt
    float r2Const = 1.0f;
    float r2Pot = 0.0f;

    float operator()(float x, float control) const noexcept {
        const float r2 = r2Const + r2Pot * control;
        const float u = thermalVoltage * std::asinh(x / (2.0f * saturationCurrent * r2));
        return std::fabs(u) > std::fabs(x) ? x : u;
    }
};

// Germanium common-emitter stage (Rangemaster): input is the junction voltage v_pi, output
// the collector swing in volts. Inverting exponential — soft cutoff ceiling +gainVolts for
// negative swings, hard collector-saturation clamp at -headroom once x reaches
// saturationInput (= vt*ln(1 + headroom/gainVolts), precomputed by the compiler; also
// guards the exp against overflow). Slope at the origin is -gainVolts/thermalVoltage.
struct GeBjtShape {
    float thermalVoltage = 1.0f; // n*Vt
    float gainVolts = 1.0f;      // Ic_q (exponential part) * RL
    float headroom = 1.0f;       // V toward saturation
    float saturationInput = 1.0f;

    float operator()(float x, float /*control*/) const noexcept {
        if (x >= saturationInput)
            return -headroom;
        return -gainVolts * (std::exp(x / thermalVoltage) - 1.0f);
    }
};

} // namespace tonestack::nodes
