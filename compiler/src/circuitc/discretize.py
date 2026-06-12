"""Bilinear discretization of the Tube Screamer LTI sub-networks.

Each linear RC/op-amp network maps to a normalized biquad section (a0 = 1) at a given pot
position and sample rate. These are the canonical TS9 filter coefficients; the symbolic
MNA front-end (added later) is validated against the committed golden they produce.

Coefficients are computed in float64 and ordered ``b0,b1,b2 / a0,a1,a2`` as ascending powers
of z^-1, matching the engine's transposed Direct Form II recurrence and ``BiquadSection`` POD.
"""

from __future__ import annotations

import math
from typing import Any

import numpy as np
from numpy.typing import NDArray
from scipy.signal import bilinear

from .bias import OperatingPoint
from .ir import BiquadSection
from .netlist import DriveSection, RangemasterCircuit, ToneSection


def _normalize(b0: float, b1: float, b2: float, a0: float, a1: float, a2: float) -> BiquadSection:
    """Scale numerator and denominator so a0 == 1."""
    return BiquadSection(b0=b0 / a0, b1=b1 / a0, b2=b2 / a0, a1=a1 / a0, a2=a2 / a0)


def drive_section(d: DriveSection, drive: float, fs: float) -> BiquadSection:
    """Drive filter biquad at pot fraction ``drive`` in [0, 1] and sample rate ``fs``."""
    fs2 = fs * fs
    rdrive = d.rpot * drive + d.rf

    b0 = 4.0 * d.c1 * d.cf * d.r1 * rdrive * fs2 + 2.0 * d.c1 * d.r1 * fs + 2.0 * d.c1 * rdrive * fs + 2.0 * d.cf * rdrive * fs + 1.0
    b1 = -8.0 * d.c1 * d.cf * d.r1 * rdrive * fs2 + 2.0
    b2 = 4.0 * d.c1 * d.cf * d.r1 * rdrive * fs2 - 2.0 * d.c1 * d.r1 * fs - 2.0 * d.c1 * rdrive * fs - 2.0 * d.cf * rdrive * fs + 1.0

    a0 = 4.0 * d.c1 * d.cf * d.r1 * rdrive * fs2 + 2.0 * d.c1 * d.r1 * fs + 2.0 * d.cf * rdrive * fs + 1.0
    a1 = -8.0 * d.c1 * d.cf * d.r1 * rdrive * fs2 + 2.0
    a2 = 4.0 * d.c1 * d.cf * d.r1 * rdrive * fs2 - 2.0 * d.c1 * d.r1 * fs - 2.0 * d.cf * rdrive * fs + 1.0

    return _normalize(b0, b1, b2, a0, a1, a2)


def tone_section(t: ToneSection, tone: float, fs: float) -> BiquadSection:
    """Tone filter biquad at pot fraction ``tone`` in [0, 1] and sample rate ``fs``.

    The pot wiper splits as ``Rpot1 = Rpot*w`` / ``Rpot2 = Rpot*(1-w)``."""
    fs2 = fs * fs
    rpot1 = t.rpot * tone
    rpot2 = t.rpot * (1.0 - tone)

    b0 = 2.0 * t.ctone * t.r10k * t.r220 * rpot1 * fs + 2.0 * t.ctone * t.r10k * t.r220 * rpot2 * fs + 2.0 * t.ctone * t.r10k * t.rf * rpot1 * fs + 2.0 * t.ctone * t.r10k * rpot1 * rpot2 * fs + t.r10k * rpot1 + t.r10k * rpot2
    b1 = 2.0 * t.r10k * rpot1 + 2.0 * t.r10k * rpot2
    b2 = -2.0 * t.ctone * t.r10k * t.r220 * rpot1 * fs - 2.0 * t.ctone * t.r10k * t.r220 * rpot2 * fs - 2.0 * t.ctone * t.r10k * t.rf * rpot1 * fs - 2.0 * t.ctone * t.r10k * rpot1 * rpot2 * fs + t.r10k * rpot1 + t.r10k * rpot2

    a0 = 4.0 * t.c4 * t.ctone * t.r10k * t.r1k * t.r220 * rpot1 * fs2 + 4.0 * t.c4 * t.ctone * t.r10k * t.r1k * t.r220 * rpot2 * fs2 + 4.0 * t.c4 * t.ctone * t.r10k * t.r1k * rpot1 * rpot2 * fs2 + 2.0 * t.c4 * t.r10k * t.r1k * rpot1 * fs + 2.0 * t.c4 * t.r10k * t.r1k * rpot2 * fs + 2.0 * t.ctone * t.r10k * t.r1k * rpot2 * fs + 2.0 * t.ctone * t.r10k * t.r220 * rpot1 * fs + 2.0 * t.ctone * t.r10k * t.r220 * rpot2 * fs + 2.0 * t.ctone * t.r10k * rpot1 * rpot2 * fs + 2.0 * t.ctone * t.r1k * t.r220 * rpot1 * fs + 2.0 * t.ctone * t.r1k * t.r220 * rpot2 * fs + 2.0 * t.ctone * t.r1k * rpot1 * rpot2 * fs + t.r10k * rpot1 + t.r10k * rpot2 + t.r1k * rpot1 + t.r1k * rpot2
    a1 = -8.0 * t.c4 * t.ctone * t.r10k * t.r1k * t.r220 * rpot1 * fs2 - 8.0 * t.c4 * t.ctone * t.r10k * t.r1k * t.r220 * rpot2 * fs2 - 8.0 * t.c4 * t.ctone * t.r10k * t.r1k * rpot1 * rpot2 * fs2 + 2.0 * t.r10k * rpot1 + 2.0 * t.r10k * rpot2 + 2.0 * t.r1k * rpot1 + 2.0 * t.r1k * rpot2
    a2 = 4.0 * t.c4 * t.ctone * t.r10k * t.r1k * t.r220 * rpot1 * fs2 + 4.0 * t.c4 * t.ctone * t.r10k * t.r1k * t.r220 * rpot2 * fs2 + 4.0 * t.c4 * t.ctone * t.r10k * t.r1k * rpot1 * rpot2 * fs2 - 2.0 * t.c4 * t.r10k * t.r1k * rpot1 * fs - 2.0 * t.c4 * t.r10k * t.r1k * rpot2 * fs - 2.0 * t.ctone * t.r10k * t.r1k * rpot2 * fs - 2.0 * t.ctone * t.r10k * t.r220 * rpot1 * fs - 2.0 * t.ctone * t.r10k * t.r220 * rpot2 * fs - 2.0 * t.ctone * t.r10k * rpot1 * rpot2 * fs - 2.0 * t.ctone * t.r1k * t.r220 * rpot1 * fs - 2.0 * t.ctone * t.r1k * t.r220 * rpot2 * fs - 2.0 * t.ctone * t.r1k * rpot1 * rpot2 * fs + t.r10k * rpot1 + t.r10k * rpot2 + t.r1k * rpot1 + t.r1k * rpot2

    return _normalize(b0, b1, b2, a0, a1, a2)


# --- Rangemaster (numeric polynomial MNA) -------------------------------------------------
#
# Unlike the TS9's frozen closed forms, the Rangemaster networks are solved numerically per
# grid point: admittances become (numerator, denominator) polynomial pairs in s (descending
# coefficients), the 3-node MNA system is solved by Cramer's rule with polynomial
# arithmetic, and the resulting rational H(s) is bilinear-transformed and split into
# second-order sections. Exact per grid point, and the stepping stone to the MNA front-end.

_Poly = NDArray[np.floating[Any]]


def _p(*coef: float) -> _Poly:
    return np.asarray(coef, dtype=np.float64)


def _pm(a: _Poly, b: _Poly) -> _Poly:
    return np.polymul(a, b)


def _det3(m: list[list[_Poly]]) -> _Poly:
    def minor(r0: int, r1: int, c0: int, c1: int) -> _Poly:
        return np.polysub(_pm(m[r0][c0], m[r1][c1]), _pm(m[r0][c1], m[r1][c0]))

    t0 = _pm(m[0][0], minor(1, 2, 1, 2))
    t1 = _pm(m[0][1], minor(1, 2, 0, 2))
    t2 = _pm(m[0][2], minor(1, 2, 0, 1))
    return np.polyadd(np.polysub(t0, t1), t2)


RM_INPUT_ORDER = 4  # Lp, C_jack, C1, Ce


def _zcoeffs(num: _Poly, den: _Poly, fs: float, order: int) -> list[float]:
    """Bilinear-transform H(s) and return normalized direct-form ``[b0..bN, a1..aN]``.
    No factoring: unfactored coefficients are polynomial in the component values, hence
    smooth across a parameter grid (factored sections regroup at root breakaways and
    cannot be interpolated continuously)."""
    bz, az = bilinear(num, den, fs)
    b = np.zeros(order + 1)
    a = np.zeros(order + 1)
    b[: len(bz)] = bz
    a[: len(az)] = az
    b /= a[0]
    a /= a[0]
    return [float(x) for x in b] + [float(x) for x in a[1:]]


def _section_from_tf(num: _Poly, den: _Poly, fs: float) -> BiquadSection:
    """First/second-order H(s) to one normalized biquad section."""
    if not np.any(num):  # degenerate all-zero numerator (e.g. a pot at zero): silence
        return BiquadSection(b0=0.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0)
    row = _zcoeffs(num, den, fs, 2)
    return BiquadSection(b0=row[0], b1=row[1], b2=row[2], a1=row[3], a2=row[4])


def rangemaster_input_tf(
    c: RangemasterCircuit,
    op: OperatingPoint,
    volume: float,
    cable: float,
    pickup: float,
) -> tuple[_Poly, _Poly]:
    """H(s) = v_pi / Vs: pickup EMF to the transistor's junction voltage, including the
    guitar volume pot, cable capacitance, C1 against the stage input impedance, the Miller
    input capacitance, and the emitter-bypass shelf. 4th order: the Miller capacitance is
    lumped onto the cable node through C1 (Cm*C1/(C1+Cm)) rather than kept as a fifth
    state — a fifth state adds a parasitic pole whose root structure reorganizes across
    the grid, which no fixed section assignment can interpolate continuously, while its
    in-band effect beyond the lumped capacitance is negligible.

    Nodes: p (pot top), j (wiper/cable), b (base). The wiper legs are clamped to 1 ohm so
    the volume endpoints stay non-singular.
    """
    t = c.transistor
    lp, rp, rvol = c.source.at(pickup)
    track = a_taper(volume)  # volume is the knob proportion; A-taper folded in here
    rv1 = max((1.0 - track) * rvol, 1.0)  # wiper to hot
    rv2 = max(track * rvol, 1.0)  # wiper to ground
    rth = c.r_bias_top * c.r_bias_bot / (c.r_bias_top + c.r_bias_bot)
    cm = t.cob * (1.0 + op.av_mid)  # Miller approximation at mid-band gain
    c_jack = cable + cm * c.c_in / (c.c_in + cm)  # cable + lumped Miller

    # Zin(s) = r_pi + (hfe+1) * (Re || 1/sCe) = Dz/Nz
    nz = _p(c.r_emitter * c.c_emitter, 1.0)
    dz = _p(op.r_pi * c.r_emitter * c.c_emitter, op.r_pi + (t.hfe + 1.0) * c.r_emitter)

    zpick = _p(lp, rp)  # Rp + sLp

    # Row p (x (Rp+sLp)*Rv1):  vp*(Rv1 + Rp+sLp) - vj*(Rp+sLp) = Rv1*Vs
    row_p = [np.polyadd(_p(rv1), zpick), _p(-lp, -rp), _p(0.0)]
    # Row j: vp*G1 - vj*(G1 + G2 + s(Cjack+C1)) + vb*sC1 = 0
    row_j = [
        _p(1.0 / rv1),
        _p(-(c_jack + c.c_in), -(1.0 / rv1 + 1.0 / rv2)),
        _p(c.c_in, 0.0),
    ]
    # Row b (x Dz): vj*sC1*Dz - vb*[(sC1 + 1/Rth)*Dz + Nz] = 0
    sc1_dz = _pm(_p(c.c_in, 0.0), dz)
    load = np.polyadd(_pm(_p(c.c_in, 1.0 / rth), dz), nz)
    row_b = [_p(0.0), sc1_dz, _pm(_p(-1.0), load)]

    m = [row_p, row_j, row_b]
    # det(m) carries one factor of Dz (row b was cleared by it); the v_pi relation
    # v_pi = vb * r_pi * Nz / Dz contributes another 1/Dz, and the Cramer numerator
    # rv1*M10*M21 contains sC1*Dz — so one Dz cancels exactly. Build num with the
    # cancellation done analytically, keeping the system at its true 4th order.
    den = _det3(m)
    num = _pm(_p(rv1), m[1][0])
    num = _pm(num, _p(c.c_in, 0.0))
    num = _pm(num, _pm(_p(op.r_pi), nz))
    return num, den


RM_INPUT_SECTIONS = 2  # shelf pole pair + resonance pair

# Audio-taper (A) pot law: track fraction at knob proportion p, ~10% at half rotation.
# Rangemaster-model knob axes are sampled uniformly in *proportion* and the taper is folded
# in here, so the engine's linear interpolation runs in the domain where adjacent rows are
# closest. (Physical-valued axes interpolate poorly across a taper's log-spaced bottom.)
_A_TAPER_EXP = math.log(0.1) / math.log(0.5)


def a_taper(p: float) -> float:
    p = min(max(p, 0.0), 1.0)
    return p**_A_TAPER_EXP if p > 0.0 else 0.0


def rangemaster_input_iir(
    c: RangemasterCircuit,
    op: OperatingPoint,
    volume: float,
    cable: float,
    pickup: float,
    fs: float,
) -> list[float]:
    """Direct-form ``[b0..b4, a1..a4]`` of the input network at one grid point."""
    if volume <= 1e-6:
        # Exact silence at the volume floor: a zero row fades cleanly under interpolation,
        # where the 1-ohm wiper clamp would otherwise leave a degenerate near-silent row.
        return [0.0] * (2 * RM_INPUT_ORDER + 1)
    num, den = rangemaster_input_tf(c, op, volume, cable, pickup)
    return _zcoeffs(num, den, fs, RM_INPUT_ORDER)


def rangemaster_output_section(c: RangemasterCircuit, boost: float, fs: float) -> BiquadSection:
    """Boost-pot wiper divider into C4 and the downstream load. First order:
    H(s) = b * s*C4*Rn / (1 + s*C4*(Rn + b(1-b)*RL))."""
    b = a_taper(boost)  # boost is the knob proportion (10KA pot)
    rsrc = b * (1.0 - b) * c.r_load
    num = _p(b * c.c_out * c.r_next, 0.0)
    den = _p(c.c_out * (c.r_next + rsrc), 1.0)
    return _section_from_tf(num, den, fs)
