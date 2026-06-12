"""IR contract tests: serialization is stable and the schema rejects malformed tables."""

from __future__ import annotations

import pytest
from pydantic import ValidationError

from circuitc.ir import BiquadParamTable, BiquadRateTable, BiquadSection, CircuitIR, ParamSpec
from circuitc.sample import build_ts9


def test_ir_json_roundtrip_is_stable() -> None:
    ir = build_ts9(rows=16)
    once = ir.model_dump_json()
    reloaded = CircuitIR.model_validate_json(once)
    assert reloaded.model_dump_json() == once


def test_axis_must_be_strictly_ascending() -> None:
    with pytest.raises(ValidationError):
        BiquadParamTable(
            id="bad",
            params=[ParamSpec(id="p", min=0.0, max=1.0)],
            axes=[[0.0, 0.5, 0.5]],  # not strictly ascending
            rates=[],
        )


def test_section_count_must_match_grid() -> None:
    section = BiquadSection(b0=1.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0)
    with pytest.raises(ValidationError):
        BiquadParamTable(
            id="bad",
            params=[ParamSpec(id="p", min=0.0, max=1.0)],
            axes=[[0.0, 1.0]],
            rates=[BiquadRateTable(sample_rate=48000.0, sections=[section])],  # 1 != 2
        )


def test_axes_must_match_params() -> None:
    with pytest.raises(ValidationError):
        BiquadParamTable(
            id="bad",
            params=[ParamSpec(id="p", min=0.0, max=1.0)],
            axes=[[0.0, 1.0], [0.0, 1.0]],  # 2 axes for 1 param
            rates=[],
        )


def test_two_axis_grid_shape_is_validated() -> None:
    section = BiquadSection(b0=1.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0)
    params = [ParamSpec(id="x", min=0.0, max=1.0), ParamSpec(id="y", min=0.0, max=1.0)]
    axes = [[0.0, 1.0], [0.0, 0.5, 1.0]]
    # 2 x 3 grid: 6 sections valid, 5 invalid.
    BiquadParamTable(
        id="ok",
        params=params,
        axes=axes,
        rates=[BiquadRateTable(sample_rate=48000.0, sections=[section] * 6)],
    )
    with pytest.raises(ValidationError):
        BiquadParamTable(
            id="bad",
            params=params,
            axes=axes,
            rates=[BiquadRateTable(sample_rate=48000.0, sections=[section] * 5)],
        )


def test_axis_count_capped_at_engine_limit() -> None:
    # 5 axes would emit a header the engine's kMaxTableAxes == 4 nodes reject at runtime.
    section = BiquadSection(b0=1.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0)
    params = [ParamSpec(id=f"p{k}", min=0.0, max=1.0) for k in range(5)]
    axes = [[0.0]] * 5
    with pytest.raises(ValidationError):
        BiquadParamTable(
            id="bad",
            params=params,
            axes=axes,
            rates=[BiquadRateTable(sample_rate=48000.0, sections=[section])],
        )


def test_extra_fields_forbidden() -> None:
    with pytest.raises(ValidationError):
        ParamSpec.model_validate({"id": "p", "min": 0.0, "max": 1.0, "bogus": 1})
