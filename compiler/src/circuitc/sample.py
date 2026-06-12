"""Pot-axis sampling: sweep each control and assemble the discretized IR.

The axis is sampled uniformly in the host's normalized proportion [0, 1] and stored in the
post-skew physical value domain — the same mapping the engine's ``Parameter.value()`` applies.
So uniform proportion sampling naturally densifies the axis where a log pot compresses, and
table rows line up with what ``BiquadNode`` looks up.
"""

from __future__ import annotations

import math

from .bias import solve_bias
from .discretize import (
    RM_INPUT_ORDER,
    drive_section,
    rangemaster_input_iir,
    rangemaster_output_section,
    tone_section,
)
from .ir import (
    BiquadParamTable,
    BiquadRateTable,
    BjtWaveshaper,
    CircuitIR,
    IirParamTable,
    IirRateTable,
    Oversample,
    ParamSpec,
    Skew,
    Stage,
    Waveshaper,
)
from .netlist import TS9, GuitarSource, RangemasterCircuit

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
    return BiquadParamTable(id="drive", params=[spec], axes=[axis], rates=rate_tables)


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
    return BiquadParamTable(id="tone", params=[spec], axes=[axis], rates=rate_tables)


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


# --- Rangemaster ---------------------------------------------------------------------------

RM_VOLUME_ROWS = 25
RM_CABLE_ROWS = 5
RM_PICKUP_ROWS = 5
RM_BOOST_ROWS = 25


def _cosine_axis(rows: int) -> list[float]:
    """Proportion-domain rows clustered toward both ends (cosine spacing). Knob axes that
    feed an audio-taper law swing hardest near the extremes — the Rangemaster's response
    moves ~20 dB across the last few percent of guitar-volume rotation — so end clustering
    buys accuracy where uniform spacing wastes rows mid-axis."""
    return [0.5 * (1.0 - math.cos(math.pi * i / (rows - 1))) for i in range(rows)]


def rm_volume_param() -> ParamSpec:
    # Axis is the knob *proportion*; the A-taper is folded into the discretizer
    # (discretize.a_taper) so the engine interpolates uniformly spaced rows.
    return ParamSpec(
        id="volume",
        min=0.0,
        max=1.0,
        skew=Skew.linear,
        skew_midpoint=0.5,
        default_proportion=1.0,
    )


def rm_cable_param(source: GuitarSource) -> ParamSpec:
    return ParamSpec(
        id="cable",
        min=source.cable_min,
        max=source.cable_max,
        skew=Skew.linear,
        skew_midpoint=0.5 * (source.cable_min + source.cable_max),
        default_proportion=0.33,
    )


def rm_pickup_param() -> ParamSpec:
    # Morph: 0 = bright single-coil rig, 1 = dark humbucker rig (see GuitarSource).
    return ParamSpec(
        id="pickup",
        min=0.0,
        max=1.0,
        skew=Skew.linear,
        skew_midpoint=0.5,
        default_proportion=0.0,
    )


def rm_boost_param() -> ParamSpec:
    # 10KA boost pot; axis is the knob proportion, A-taper folded into the discretizer.
    return ParamSpec(
        id="boost",
        min=0.0,
        max=1.0,
        skew=Skew.linear,
        skew_midpoint=0.5,
        default_proportion=1.0,
    )


def build_rangemaster_input_table(
    c: RangemasterCircuit, rates: tuple[float, ...]
) -> IirParamTable:
    """One 4th-order direct-form grid over axes [volume, cable, pickup]."""
    op = solve_bias(c)
    params = [rm_volume_param(), rm_cable_param(c.source), rm_pickup_param()]
    axes = [
        _cosine_axis(RM_VOLUME_ROWS),
        _axis(params[1], RM_CABLE_ROWS),
        _axis(params[2], RM_PICKUP_ROWS),
    ]
    per_rate: dict[float, list[list[float]]] = {fs: [] for fs in rates}
    for volume in axes[0]:
        for cable in axes[1]:
            for pickup in axes[2]:
                for fs in rates:
                    per_rate[fs].append(
                        rangemaster_input_iir(c, op, volume, cable, pickup, fs)
                    )
    return IirParamTable(
        id="input",
        order=RM_INPUT_ORDER,
        params=params,
        axes=axes,
        rates=[IirRateTable(sample_rate=fs, rows=per_rate[fs]) for fs in rates],
    )


def build_rangemaster_output_table(
    c: RangemasterCircuit, rates: tuple[float, ...]
) -> BiquadParamTable:
    spec = rm_boost_param()
    axis = _cosine_axis(RM_BOOST_ROWS)
    return BiquadParamTable(
        id="output",
        params=[spec],
        axes=[axis],
        rates=[
            BiquadRateTable(
                sample_rate=fs,
                sections=[rangemaster_output_section(c, b, fs) for b in axis],
            )
            for fs in rates
        ],
    )


def build_rangemaster_stage(c: RangemasterCircuit) -> BjtWaveshaper:
    op = solve_bias(c)
    gain = op.ic_exp * c.r_load
    sat_in = c.transistor.nvt * math.log(1.0 + op.headroom / gain)
    return BjtWaveshaper(
        id="stage",
        thermal_voltage=c.transistor.nvt,
        gain_volts=gain,
        headroom=op.headroom,
        saturation_input=sat_in,
    )


def build_rangemaster(
    c: RangemasterCircuit | None = None,
    rates: tuple[float, ...] = DEFAULT_RATES,
    factor: int = OVERSAMPLE_FACTOR,
) -> CircuitIR:
    """The full Rangemaster: source/input grid -> oversampled germanium stage -> boost tap."""
    c = c or RangemasterCircuit()
    stages: list[Stage] = [build_rangemaster_input_table(c, rates)]
    stages.append(Oversample(factor=factor, inner=[build_rangemaster_stage(c)]))
    stages.append(build_rangemaster_output_table(c, rates))
    return CircuitIR(name="rangemaster", sample_rates=list(rates), stages=stages)
