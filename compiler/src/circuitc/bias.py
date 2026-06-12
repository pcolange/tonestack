"""DC operating point and small-signal model for the Rangemaster's germanium stage.

The factory biasing procedure selected transistors until the collector sat at the
documented ideal (|Vc| = 7.0 V), so the operating point is pinned there and everything
else is derived. Leakage model: ``Ic = Ic_exp + (hfe+1)*Icbo`` with only ``Ic_exp``
(the exponential transport part) modulating with signal; ``Ib = Ic_exp/hfe - Icbo``.

With Ic pinned, the base KVL is linear, so the point is closed-form. The implied Vbe/Is
are bookkeeping artifacts of the pin (real germanium Vbe sits lower); the audio-relevant
quantities — Ic_exp, gm, r_pi, and the collector headroom — do not depend on them.
"""

from __future__ import annotations

from dataclasses import dataclass

from .netlist import RangemasterCircuit


@dataclass(frozen=True)
class OperatingPoint:
    ic_exp: float  # A, exponential (modulating) part of Ic
    ic: float  # A, total collector current incl. leakage
    vc: float  # V, collector magnitude below the supply rail
    vbe: float  # V, implied by the pin (bookkeeping only)
    gm: float  # S, Ic_exp / nVt
    r_pi: float  # ohm, hfe / gm
    av_mid: float  # mid-band magnitude of the stage gain gm * RL
    headroom: float  # V, swing available toward saturation: vc - vce_sat


def solve_bias(c: RangemasterCircuit) -> OperatingPoint:
    t = c.transistor
    vth = c.supply * c.r_bias_bot / (c.r_bias_top + c.r_bias_bot)
    rth = c.r_bias_top * c.r_bias_bot / (c.r_bias_top + c.r_bias_bot)

    # Pin Ic so the collector lands on the factory-ideal point.
    ic_total = (c.supply - c.vc_target) / c.r_load
    ic_exp = ic_total - (t.hfe + 1.0) * t.icbo
    if ic_exp <= 0.0:
        raise ValueError("rangemaster bias: leakage exceeds the target collector current")

    ib = ic_exp / t.hfe - t.icbo
    ie = ic_total + ib
    vbe = vth - ib * rth - ie * c.r_emitter

    gm = ic_exp / t.nvt
    return OperatingPoint(
        ic_exp=ic_exp,
        ic=ic_total,
        vc=c.vc_target,
        vbe=vbe,
        gm=gm,
        r_pi=t.hfe / gm,
        av_mid=gm * c.r_load,
        headroom=c.vc_target - t.vce_sat,
    )
