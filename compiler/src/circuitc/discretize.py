"""Bilinear discretization of the Tube Screamer LTI sub-networks.

Each linear RC/op-amp network maps to a normalized biquad section (a0 = 1) at a given pot
position and sample rate. These are the canonical TS9 filter coefficients; the symbolic
MNA front-end (added later) is validated against the committed golden they produce.

Coefficients are computed in float64 and ordered ``b0,b1,b2 / a0,a1,a2`` as ascending powers
of z^-1, matching the engine's transposed Direct Form II recurrence and ``BiquadSection`` POD.
"""

from __future__ import annotations

from .ir import BiquadSection
from .netlist import DriveSection, ToneSection


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
