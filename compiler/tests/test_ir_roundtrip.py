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
            param=ParamSpec(id="p", min=0.0, max=1.0),
            param_axis=[0.0, 0.5, 0.5],  # not strictly ascending
            rates=[],
        )


def test_section_count_must_match_axis() -> None:
    section = BiquadSection(b0=1.0, b1=0.0, b2=0.0, a1=0.0, a2=0.0)
    with pytest.raises(ValidationError):
        BiquadParamTable(
            id="bad",
            param=ParamSpec(id="p", min=0.0, max=1.0),
            param_axis=[0.0, 1.0],
            rates=[BiquadRateTable(sample_rate=48000.0, sections=[section])],  # 1 != 2
        )


def test_extra_fields_forbidden() -> None:
    with pytest.raises(ValidationError):
        ParamSpec.model_validate({"id": "p", "min": 0.0, "max": 1.0, "bogus": 1})
