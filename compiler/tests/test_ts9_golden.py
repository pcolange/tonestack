"""Golden regression for the TS9 discretizer.

``discretize.py`` is the canonical source of the TS9 filter coefficients. These tests pin it
three ways:

1. against frozen literal coefficients (an independent anchor — break the long formulas and
   this fails regardless of the committed JSON);
2. against the committed golden IR in ``contract/golden`` (regenerate deliberately with
   ``circuitc compile`` when the math intentionally changes);
3. by pole stability across the whole swept table.

When the symbolic MNA path lands, it must match the same committed golden.
"""

from __future__ import annotations

import json
from pathlib import Path

import numpy as np

from circuitc.discretize import drive_section, tone_section
from circuitc.ir import BiquadParamTable, BiquadSection, iter_biquads
from circuitc.netlist import TS9
from circuitc.sample import build_ts9

GOLDEN_DIR = Path(__file__).resolve().parents[2] / "contract" / "golden"
TOL = 1e-9


def test_drive_frozen_reference_point() -> None:
    # drive=0.5, fs=192000 (48k base x4 oversample). Frozen from the float64 formula with the
    # factory 500k drive pot.
    s = drive_section(TS9().drive, 0.5, 192000.0)
    assert s.b0 == 10.18032517454916
    assert s.b1 == -1.6866225777295742
    assert s.b2 == -8.486942973773365
    assert s.a1 == -1.6866225777295742
    assert s.a2 == 0.693382200775795


def test_tone_frozen_reference_point() -> None:
    # tone=0.5, fs=48000. Frozen from the float64 formula.
    s = tone_section(TS9().tone, 0.5, 48000.0)
    assert s.b0 == 0.04906691142738325
    assert s.b1 == 0.0008056540777394825
    assert s.b2 == -0.04826125734964377
    assert s.a1 == -1.8750019319584785
    assert s.a2 == 0.8767743709295053


def _load_committed(table_id: str) -> BiquadParamTable:
    path = GOLDEN_DIR / f"ts9_{table_id}.coeffs.json"
    return BiquadParamTable.model_validate(json.loads(path.read_text()))


def _fresh(table_id: str) -> BiquadParamTable:
    for table in iter_biquads(build_ts9().stages):
        if table.id == table_id:
            return table
    raise KeyError(table_id)


def _sections_close(sa: BiquadSection, sb: BiquadSection) -> bool:
    return (
        abs(sa.b0 - sb.b0) <= TOL
        and abs(sa.b1 - sb.b1) <= TOL
        and abs(sa.b2 - sb.b2) <= TOL
        and abs(sa.a1 - sb.a1) <= TOL
        and abs(sa.a2 - sb.a2) <= TOL
    )


def _assert_table_close(a: BiquadParamTable, b: BiquadParamTable) -> None:
    assert a.axes == b.axes
    assert len(a.rates) == len(b.rates)
    for ra, rb in zip(a.rates, b.rates, strict=True):
        assert ra.sample_rate == rb.sample_rate
        for sa, sb in zip(ra.sections, rb.sections, strict=True):
            assert _sections_close(sa, sb)


def test_drive_matches_committed_golden() -> None:
    _assert_table_close(_fresh("drive"), _load_committed("drive"))


def test_tone_matches_committed_golden() -> None:
    _assert_table_close(_fresh("tone"), _load_committed("tone"))


def test_all_poles_stable() -> None:
    for table in iter_biquads(build_ts9().stages):
        for rate in table.rates:
            for s in rate.sections:
                roots = np.roots(np.array([1.0, s.a1, s.a2]))
                max_radius = float(np.max(np.abs(roots)))
                assert max_radius < 1.0, f"{table.id} @ {rate.sample_rate} unstable"


def test_interpolated_midpoints_stable() -> None:
    # BiquadNode lerps coefficients between rows; stability at the rows does not imply
    # stability between them, so check every adjacent-row midpoint too.
    for table in iter_biquads(build_ts9().stages):
        for rate in table.rates:
            for s0, s1 in zip(rate.sections, rate.sections[1:], strict=False):
                a1 = 0.5 * (s0.a1 + s1.a1)
                a2 = 0.5 * (s0.a2 + s1.a2)
                roots = np.roots(np.array([1.0, a1, a2]))
                max_radius = float(np.max(np.abs(roots)))
                assert max_radius < 1.0, f"{table.id} @ {rate.sample_rate} midpoint unstable"


def _magnitude(s: BiquadSection, freq: float, fs: float) -> float:
    zi = np.exp(-2j * np.pi * freq / fs)
    num = s.b0 + s.b1 * zi + s.b2 * zi * zi
    den = 1.0 + s.a1 * zi + s.a2 * zi * zi
    return float(abs(num / den))


def test_tone_pot_shapes_treble() -> None:
    # Outcome check: tone rolled down darkens the signal (4 kHz falls relative to 200 Hz);
    # tone up brightens it.
    fs = 48000.0
    dark = tone_section(TS9().tone, 0.0, fs)
    bright = tone_section(TS9().tone, 1.0, fs)
    dark_ratio = _magnitude(dark, 4000.0, fs) / _magnitude(dark, 200.0, fs)
    bright_ratio = _magnitude(bright, 4000.0, fs) / _magnitude(bright, 200.0, fs)
    assert dark_ratio < bright_ratio
    assert dark_ratio < 1.0
