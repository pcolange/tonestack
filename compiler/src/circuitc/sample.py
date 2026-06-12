"""Pot-axis sampling: sweep each control and assemble the discretized IR.

The axis is sampled uniformly in the host's normalized proportion [0, 1] and stored in the
post-skew physical value domain — the same mapping the engine's ``Parameter.value()`` applies.
So uniform proportion sampling naturally densifies the axis where a log pot compresses, and
table rows line up with what ``BiquadNode`` looks up.
"""

from __future__ import annotations

import math

from .discretize import drive_section, tone_section
from .ir import (
    BiquadParamTable,
    BiquadRateTable,
    CircuitIR,
    Oversample,
    ParamSpec,
    Skew,
    Waveshaper,
)
from .netlist import TS9

# Base sample rates the engine ships tables for.
DEFAULT_RATES: tuple[float, ...] = (44100.0, 48000.0, 88200.0, 96000.0)
DEFAULT_ROWS = 64
OVERSAMPLE_FACTOR = 4


def _skew_factor(spec: ParamSpec) -> float:
    """Skew factor such that value(proportion 0.5) == skew_midpoint (the engine's convention)."""
    if spec.skew is not Skew.logarithmic:
        return 1.0
    span = spec.max - spec.min
    if span <= 0.0:
        return 1.0
    mid_frac = (spec.skew_midpoint - spec.min) / span
    if 0.0 < mid_frac < 1.0:
        return math.log(0.5) / math.log(mid_frac)
    return 1.0


def _axis(spec: ParamSpec, rows: int) -> list[float]:
    """Physical values for ``rows`` proportions uniform on [0, 1]."""
    sf = _skew_factor(spec)
    out: list[float] = []
    for i in range(rows):
        p = i / (rows - 1) if rows > 1 else 0.0
        eff = p ** (1.0 / sf) if (sf != 1.0 and p > 0.0) else p
        out.append(spec.min + (spec.max - spec.min) * eff)
    return out


def drive_param() -> ParamSpec:
    # Factory drive pot is 500KA (audio taper): ~10% of track at half rotation, modeled as
    # log skew with midpoint 0.1 (see circuits/REFERENCE.md).
    return ParamSpec(
        id="drive",
        min=0.0,
        max=1.0,
        skew=Skew.logarithmic,
        skew_midpoint=0.1,
        default_proportion=0.5,
    )


def tone_param() -> ParamSpec:
    # Factory tone pot is 20KG (symmetric S-curve): 50% of track at half rotation. Linear
    # skew matches the curve at the midpoint, the closest representable approximation.
    return ParamSpec(
        id="tone",
        min=0.0,
        max=1.0,
        skew=Skew.linear,
        skew_midpoint=0.5,
        default_proportion=0.5,
    )


def build_drive_table(
    ts: TS9, rates: tuple[float, ...], rows: int, factor: int
) -> BiquadParamTable:
    spec = drive_param()
    axis = _axis(spec, rows)
    rate_tables = [
        BiquadRateTable(
            sample_rate=base * factor,
            sections=[drive_section(ts.drive, v, base * factor) for v in axis],
        )
        for base in rates
    ]
    return BiquadParamTable(id="drive", param=spec, param_axis=axis, rates=rate_tables)


def build_tone_table(ts: TS9, rates: tuple[float, ...], rows: int) -> BiquadParamTable:
    spec = tone_param()
    axis = _axis(spec, rows)
    rate_tables = [
        BiquadRateTable(
            sample_rate=base,
            sections=[tone_section(ts.tone, v, base) for v in axis],
        )
        for base in rates
    ]
    return BiquadParamTable(id="tone", param=spec, param_axis=axis, rates=rate_tables)


def build_waveshaper(ts: TS9) -> Waveshaper:
    return Waveshaper(
        id="clipper",
        saturation_current=ts.clipper.saturation_current,
        thermal_voltage=ts.clipper.thermal_voltage,
        r2_param="drive",
        r2_const=ts.drive.rf,
        r2_pot=ts.drive.rpot,
    )


def build_ts9(
    ts: TS9 | None = None,
    rates: tuple[float, ...] = DEFAULT_RATES,
    rows: int = DEFAULT_ROWS,
    factor: int = OVERSAMPLE_FACTOR,
) -> CircuitIR:
    """The full Tube Screamer: oversampled { drive biquad -> asinh clipper } -> tone biquad."""
    ts = ts or TS9()
    drive = build_drive_table(ts, rates, rows, factor)
    tone = build_tone_table(ts, rates, rows)
    clipper = build_waveshaper(ts)
    return CircuitIR(
        name="ts9",
        sample_rates=list(rates),
        stages=[
            Oversample(factor=factor, inner=[drive, clipper]),
            tone,
        ],
    )
