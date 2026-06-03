"""The compiler <-> engine data contract (the IR), pydantic as the single source of truth.

A compiled circuit is a chain of stages. Linear sub-networks become ``BiquadParamTable``
stages: discretized biquad coefficients sampled across a pot axis, one section array per
supported sample rate. Nonlinear clippers become ``Waveshaper`` stages carrying the analytic
parameters only. An ``Oversample`` stage wraps an inner chain run at N x.

The engine never evaluates a coefficient expression at runtime: it interpolates between table
rows. All symbolic/algebraic work stays here, offline.
"""

from __future__ import annotations

from collections.abc import Iterator
from enum import Enum
from typing import Annotated, Literal

from pydantic import BaseModel, ConfigDict, Field, model_validator

# Semver. The C++ loader rejects a major-version mismatch.
IR_VERSION = "0.1.0"


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


class BiquadSection(_Strict):
    """One second-order section, a0 normalized to 1. Field order matches the C++
    ``tonestack::nodes::BiquadSection`` POD and the transposed-DF-II recurrence."""

    b0: float
    b1: float
    b2: float
    a1: float
    a2: float


class BiquadRateTable(_Strict):
    """Per-sample-rate coefficient rows; ``sections[i]`` is the section at ``param_axis[i]``."""

    sample_rate: float
    sections: list[BiquadSection]


class BiquadParamTable(_Strict):
    type: Literal["biquad_param_table"] = "biquad_param_table"
    id: str
    param: ParamSpec
    param_axis: list[float]  # ascending physical values
    rates: list[BiquadRateTable]

    @model_validator(mode="after")
    def _check_shapes(self) -> BiquadParamTable:
        n = len(self.param_axis)
        if n < 1:
            raise ValueError(f"{self.id}: param_axis must be non-empty")
        if any(b <= a for a, b in zip(self.param_axis, self.param_axis[1:], strict=False)):
            raise ValueError(f"{self.id}: param_axis must be strictly ascending")
        for rate in self.rates:
            if len(rate.sections) != n:
                raise ValueError(
                    f"{self.id} @ {rate.sample_rate}: "
                    f"{len(rate.sections)} sections for {n} axis points"
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


class Oversample(_Strict):
    type: Literal["oversample"] = "oversample"
    factor: int = 4
    inner: list[Stage]


Stage = Annotated[
    BiquadParamTable | Waveshaper | Oversample,
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


def iter_waveshapers(stages: list[Stage]) -> Iterator[Waveshaper]:
    for stage in stages:
        if isinstance(stage, Waveshaper):
            yield stage
        elif isinstance(stage, Oversample):
            yield from iter_waveshapers(stage.inner)
