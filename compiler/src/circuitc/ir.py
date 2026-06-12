"""The compiler <-> engine data contract (the IR), pydantic as the single source of truth.

A compiled circuit is a chain of stages. Linear sub-networks become ``BiquadParamTable``
stages: discretized biquad coefficients sampled across a pot axis, one section array per
supported sample rate. Nonlinear clippers become ``Waveshaper`` stages carrying the analytic
parameters only. An ``Oversample`` stage wraps an inner chain run at N x.

The engine never evaluates a coefficient expression at runtime: it interpolates between table
rows. All symbolic/algebraic work stays here, offline.
"""

from __future__ import annotations

import math
from collections.abc import Iterator
from enum import Enum
from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator

# Semver. The C++ loader rejects a major-version mismatch.
IR_VERSION = "0.3.0"


class _Strict(BaseModel):
    model_config = ConfigDict(extra="forbid")


class Skew(str, Enum):
    linear = "linear"
    logarithmic = "logarithmic"


class ParamSpec(_Strict):
    """A pot/control parameter. Its physical ``value`` (post-skew, in [min, max]) indexes the
    biquad table axis; the engine applies the same skew in ``Parameter.value()``."""

    id: str
    min: float
    max: float
    skew: Skew = Skew.linear
    skew_midpoint: float = 0.5
    default_proportion: float = 0.0
    smoothing_seconds: float = 0.02


class BiquadSection(_Strict):
    """One second-order section, a0 normalized to 1. Field order matches the C++
    ``tonestack::nodes::BiquadSection`` POD and the transposed-DF-II recurrence."""

    b0: float
    b1: float
    b2: float
    a1: float
    a2: float

    @model_validator(mode="after")
    def _check_finite(self) -> BiquadSection:
        for name in ("b0", "b1", "b2", "a1", "a2"):
            if not math.isfinite(getattr(self, name)):
                raise ValueError(f"non-finite coefficient {name}")
        return self


class BiquadRateTable(_Strict):
    """Per-sample-rate coefficient grid, flattened row-major over the table's axes (last
    axis fastest); ``sections[flat_index(i0..iN)]`` is the section at those axis points."""

    sample_rate: float
    sections: list[BiquadSection]


class BiquadParamTable(_Strict):
    """Coefficient grid over one or more parameter axes. ``axes[k]`` holds the ascending
    physical values for ``params[k]``; the engine multilinearly interpolates the grid."""

    type: Literal["biquad_param_table"] = "biquad_param_table"
    id: str
    params: list[ParamSpec]
    axes: list[list[float]]  # axes[k]: ascending physical values for params[k]
    rates: list[BiquadRateTable]

    @model_validator(mode="after")
    def _check_shapes(self) -> BiquadParamTable:
        if len(self.params) < 1:
            raise ValueError(f"{self.id}: at least one parameter axis required")
        if len(self.axes) != len(self.params):
            raise ValueError(
                f"{self.id}: {len(self.axes)} axes for {len(self.params)} params"
            )
        n = 1
        for k, axis in enumerate(self.axes):
            if len(axis) < 1:
                raise ValueError(f"{self.id}: axis {k} must be non-empty")
            if any(not math.isfinite(v) for v in axis):
                raise ValueError(f"{self.id}: axis {k} must be finite")
            if any(b <= a for a, b in zip(axis, axis[1:], strict=False)):
                raise ValueError(f"{self.id}: axis {k} must be strictly ascending")
            n *= len(axis)
        for rate in self.rates:
            if len(rate.sections) != n:
                raise ValueError(
                    f"{self.id} @ {rate.sample_rate}: "
                    f"{len(rate.sections)} sections for {n} grid points"
                )
        return self


class Waveshaper(_Strict):
    """Stateless nonlinear clipper split out of the LTI network. The asinh diode/op-amp soft
    clipper: ``U = nvt*asinh(x / (2*Is*R2))`` with ``R2 = r2_const + r2_pot * <r2_param value>``,
    clamped so ``|U| <= |x|`` then summed back onto the dry signal (clamp ``abs_input``)."""

    type: Literal["waveshaper"] = "waveshaper"
    id: str
    kind: Literal["asinh_diode"] = "asinh_diode"
    saturation_current: float  # Is
    thermal_voltage: float  # nvt
    r2_param: str
    r2_const: float
    r2_pot: float
    clamp: Literal["abs_input"] = "abs_input"


class IirRateTable(_Strict):
    """Per-sample-rate direct-form coefficient grid, flattened row-major over the table's
    axes; ``rows[flat_index]`` holds ``[b0..bN, a1..aN]`` (a0 normalized to 1)."""

    sample_rate: float
    rows: list[list[float]]


class IirParamTable(_Strict):
    """Direct-form IIR coefficient grid over parameter axes, for networks whose factored
    (per-section) form cannot be interpolated continuously: unfactored coefficients are
    polynomial in the component values, hence smooth across the grid. Order stays low and
    poles well-separated, so direct form is float-safe (see the engine's IirNode)."""

    type: Literal["iir_param_table"] = "iir_param_table"
    id: str
    order: int
    params: list[ParamSpec]
    axes: list[list[float]]
    rates: list[IirRateTable]

    @model_validator(mode="after")
    def _check_shapes(self) -> IirParamTable:
        if not 1 <= self.order <= 8:
            raise ValueError(f"{self.id}: order must be in [1, 8]")
        if len(self.params) < 1:
            raise ValueError(f"{self.id}: at least one parameter axis required")
        if len(self.axes) != len(self.params):
            raise ValueError(f"{self.id}: {len(self.axes)} axes for {len(self.params)} params")
        n = 1
        for k, axis in enumerate(self.axes):
            if len(axis) < 1:
                raise ValueError(f"{self.id}: axis {k} must be non-empty")
            if any(not math.isfinite(v) for v in axis):
                raise ValueError(f"{self.id}: axis {k} must be finite")
            if any(b <= a for a, b in zip(axis, axis[1:], strict=False)):
                raise ValueError(f"{self.id}: axis {k} must be strictly ascending")
            n *= len(axis)
        width = 2 * self.order + 1
        for rate in self.rates:
            if len(rate.rows) != n:
                raise ValueError(
                    f"{self.id} @ {rate.sample_rate}: {len(rate.rows)} rows for {n} grid points"
                )
            for row in rate.rows:
                if len(row) != width:
                    raise ValueError(f"{self.id} @ {rate.sample_rate}: row width != {width}")
                if any(not math.isfinite(v) for v in row):
                    raise ValueError(f"{self.id} @ {rate.sample_rate}: non-finite coefficient")
        return self


class BjtWaveshaper(_Strict):
    """Static germanium common-emitter nonlinearity (Rangemaster). Operates on the junction
    voltage v_pi delivered by the preceding LTI stage and emits the collector swing in
    volts: ``y = -gain_volts * (exp(x / thermal_voltage) - 1)``, hard-clamped at
    ``-headroom`` once ``x >= saturation_input`` (collector saturation); the exponential
    itself provides the soft cutoff ceiling ``+gain_volts``. Slope at the origin is
    ``-gain_volts / thermal_voltage`` = -gm*RL, the stage's mid-band gain."""

    type: Literal["bjt_waveshaper"] = "bjt_waveshaper"
    id: str
    kind: Literal["ge_common_emitter"] = "ge_common_emitter"
    thermal_voltage: float  # n*Vt
    gain_volts: float  # Ic_exp * RL
    headroom: float  # V available toward saturation
    saturation_input: float  # vt * ln(1 + headroom/gain_volts)


class Oversample(_Strict):
    type: Literal["oversample"] = "oversample"
    factor: int = 4
    inner: list[Stage]


Stage = Annotated[
    BiquadParamTable | IirParamTable | Waveshaper | BjtWaveshaper | Oversample,
    Field(discriminator="type"),
]


class CircuitIR(_Strict):
    ir_version: str = IR_VERSION
    name: str
    sample_rates: list[float]
    stages: list[Stage]


Oversample.model_rebuild()


def iter_biquads(stages: list[Stage]) -> Iterator[BiquadParamTable]:
    """Depth-first walk yielding every biquad table (descending into oversample stages)."""
    for stage in stages:
        if isinstance(stage, BiquadParamTable):
            yield stage
        elif isinstance(stage, Oversample):
            yield from iter_biquads(stage.inner)


def iter_iir_tables(stages: list[Stage]) -> Iterator[IirParamTable]:
    for stage in stages:
        if isinstance(stage, IirParamTable):
            yield stage
        elif isinstance(stage, Oversample):
            yield from iter_iir_tables(stage.inner)


def iter_waveshapers(stages: list[Stage]) -> Iterator[Waveshaper]:
    for stage in stages:
        if isinstance(stage, Waveshaper):
            yield stage
        elif isinstance(stage, Oversample):
            yield from iter_waveshapers(stage.inner)


def iter_bjt_waveshapers(stages: list[Stage]) -> Iterator[BjtWaveshaper]:
    for stage in stages:
        if isinstance(stage, BjtWaveshaper):
            yield stage
        elif isinstance(stage, Oversample):
            yield from iter_bjt_waveshapers(stage.inner)
