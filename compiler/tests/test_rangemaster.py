"""Rangemaster model tests: frozen reference points, committed-golden regression, grid
stability (including interpolation blends along every axis), and outcome checks for the
behaviors the pedal is famous for."""

from __future__ import annotations

import cmath
import json
import math
from functools import lru_cache
from pathlib import Path

import numpy as np

from circuitc.bias import solve_bias
from circuitc.discretize import (
    rangemaster_input_iir,
    rangemaster_input_tf,
    rangemaster_output_section,
)
from circuitc.ir import CircuitIR, IirParamTable, iter_biquads, iter_iir_tables
from circuitc.netlist import RangemasterCircuit
from circuitc.sample import build_rangemaster

GOLDEN_DIR = Path(__file__).resolve().parents[2] / "contract" / "golden"
TOL = 1e-9


@lru_cache(maxsize=1)
def _ir() -> CircuitIR:
    return build_rangemaster()


def _row_mag(row: list[float], f: float, fs: float, order: int = 4) -> float:
    z = cmath.exp(2j * math.pi * f / fs)
    num = sum(row[i] / z**i for i in range(order + 1))
    den = 1 + sum(row[order + 1 + i] / z ** (i + 1) for i in range(order))
    return abs(num / den)


def _true_input_mag(c: RangemasterCircuit, vol: float, cab: float, pk: float, f: float) -> float:
    """Analog-domain response of the input network."""
    num, den = rangemaster_input_tf(c, solve_bias(c), vol, cab, pk)
    s = 2j * math.pi * f
    return abs(complex(np.polyval(num, s)) / complex(np.polyval(den, s)))


def test_bias_frozen_operating_point() -> None:
    op = solve_bias(RangemasterCircuit())
    assert op.ic_exp == 0.00015450000000000001
    assert op.gm == 0.005942307692307693
    assert op.r_pi == 15145.631067961163
    assert op.av_mid == 59.42307692307693
    assert op.headroom == 6.8


def test_input_frozen_reference_point() -> None:
    # volume=1, cable=470p, pickup=0.5, fs=48k. Frozen from the polynomial-MNA pipeline
    # (direct-form coefficients, no factoring).
    c = RangemasterCircuit()
    row = rangemaster_input_iir(c, solve_bias(c), 1.0, 4.7e-10, 0.5, 48000.0)
    expected = [
        0.01512614642638094,
        1.7190951626204345e-06,
        -0.03025057375759925,
        -1.7190951626204345e-06,
        0.015124427331218316,
        -2.8558826190219206,
        2.717264235689191,
        -0.8440002930940668,
        -0.01736649750033433,
    ]
    assert len(row) == len(expected)
    for got, want in zip(row, expected, strict=True):
        assert abs(got - want) <= TOL


def test_output_frozen_reference_point() -> None:
    # boost is the knob proportion; 0.5 lands on the A-taper's ~10% track point.
    c = RangemasterCircuit()
    s = rangemaster_output_section(c, 0.5, 48000.0)
    assert abs(s.b0 - 0.09980620960967454) <= TOL
    assert abs(s.b1 - -0.09980620960967454) <= TOL
    assert abs(s.a1 - -0.9979207039664652) <= TOL


def test_matches_committed_golden() -> None:
    ir = _ir()
    committed_iir = IirParamTable.model_validate(
        json.loads((GOLDEN_DIR / "rangemaster_input.coeffs.json").read_text())
    )
    fresh_iir = next(iter_iir_tables(ir.stages))
    assert fresh_iir.order == committed_iir.order
    assert fresh_iir.axes == committed_iir.axes
    for ra, rb in zip(fresh_iir.rates, committed_iir.rates, strict=True):
        assert ra.sample_rate == rb.sample_rate
        for rowa, rowb in zip(ra.rows, rb.rows, strict=True):
            for va, vb in zip(rowa, rowb, strict=True):
                assert abs(va - vb) <= TOL

    from circuitc.ir import BiquadParamTable

    committed_out = BiquadParamTable.model_validate(
        json.loads((GOLDEN_DIR / "rangemaster_output.coeffs.json").read_text())
    )
    fresh_out = next(t for t in iter_biquads(ir.stages) if t.id == "output")
    assert fresh_out.axes == committed_out.axes
    for ra, rb in zip(fresh_out.rates, committed_out.rates, strict=True):
        for sa, sb in zip(ra.sections, rb.sections, strict=True):
            assert abs(sa.b0 - sb.b0) <= TOL
            assert abs(sa.a1 - sb.a1) <= TOL
            assert abs(sa.a2 - sb.a2) <= TOL


def _stable_den(den_tail: list[float]) -> bool:
    roots = np.roots(np.array([1.0, *den_tail]))
    return float(np.max(np.abs(roots))) < 1.0 if len(roots) else True


def _grid_geometry(table: IirParamTable) -> tuple[list[int], list[int]]:
    dims = [len(a) for a in table.axes]
    strides = [1] * len(dims)
    for k in range(len(dims) - 2, -1, -1):
        strides[k] = strides[k + 1] * dims[k + 1]
    return dims, strides


def test_all_grid_points_stable() -> None:
    ir = _ir()
    table = next(iter_iir_tables(ir.stages))
    order = table.order
    for rate in table.rates:
        for row in rate.rows:
            assert _stable_den(row[order + 1 :]), f"input @ {rate.sample_rate}"
    for biq in iter_biquads(ir.stages):
        for rate in biq.rates:
            for s in rate.sections:
                assert _stable_den([s.a1, s.a2]), f"{biq.id} @ {rate.sample_rate}"


def test_axis_blend_stability() -> None:
    # The engine blends adjacent grid rows linearly; the direct-form stability region is
    # not convex for order > 2, so every adjacent-row midpoint blend is checked.
    table = next(iter_iir_tables(_ir().stages))
    order = table.order
    dims, strides = _grid_geometry(table)
    total = math.prod(dims)
    for rate in table.rates:
        rows = rate.rows
        for k, (dim, stride) in enumerate(zip(dims, strides, strict=True)):
            for flat in range(total):
                if (flat // stride) % dim + 1 >= dim:
                    continue
                blend = [
                    0.5 * (a + b)
                    for a, b in zip(rows[flat], rows[flat + stride], strict=True)
                ]
                assert _stable_den(blend[order + 1 :]), (
                    f"@{rate.sample_rate} axis {k} flat {flat}"
                )


def test_interpolated_midpoint_tracks_true_response() -> None:
    # At every volume-axis midpoint the blended row must track the true network response
    # (the volume axis is the densest and moves the response hardest). Tolerance covers
    # interpolation error plus bilinear warp at the highest probe frequency.
    c = RangemasterCircuit()
    fs = 48000.0
    table = next(iter_iir_tables(_ir().stages))
    dims, strides = _grid_geometry(table)
    rate_idx = next(i for i, r in enumerate(table.rates) if r.sample_rate == fs)
    rows = table.rates[rate_idx].rows
    center = [0, dims[1] // 2, dims[2] // 2]
    cable = table.axes[1][center[1]]
    pickup = table.axes[2][center[2]]
    for i in range(dims[0] - 1):
        flat0 = i * strides[0] + center[1] * strides[1] + center[2]
        flat1 = flat0 + strides[0]
        blend = [0.5 * (a + b) for a, b in zip(rows[flat0], rows[flat1], strict=True)]
        vm = 0.5 * (table.axes[0][i] + table.axes[0][i + 1])
        for f in (500.0, 1000.0, 4000.0, 8000.0):
            want = _true_input_mag(c, vm, cable, pickup, f)
            if want < 1e-3:
                continue  # sub-audible volume floor: the table fades to the silence row
            got = _row_mag(blend, f, fs)
            err_db = abs(20.0 * math.log10(max(got, 1e-12) / want))
            assert err_db < 2.0, f"vol interval {i} @ {f} Hz: {err_db:.2f} dB"


def test_treble_boost_outcome() -> None:
    # Full volume, short cable, single coil: the net gain (input network x stage slope)
    # peaks ~20-30 dB in the upper mids and sits far lower at 100 Hz.
    c = RangemasterCircuit()
    av = solve_bias(c).av_mid
    peak_db = max(
        20.0 * math.log10(_true_input_mag(c, 1.0, 2e-10, 0.0, float(f)) * av)
        for f in range(500, 6001, 100)
    )
    low_db = 20.0 * math.log10(_true_input_mag(c, 1.0, 2e-10, 0.0, 100.0) * av)
    assert 20.0 < peak_db < 32.0
    assert peak_db - low_db > 15.0  # it is a *treble* booster


def test_volume_cleanup_outcome() -> None:
    c = RangemasterCircuit()
    full = _true_input_mag(c, 1.0, 4.7e-10, 0.0, 3000.0)
    half = _true_input_mag(c, 0.5, 4.7e-10, 0.0, 3000.0)
    assert 20.0 * math.log10(full / half) > 6.0


def test_pickup_morph_darkens_outcome() -> None:
    c = RangemasterCircuit()
    bright = _true_input_mag(c, 1.0, 4.7e-10, 0.0, 6000.0) / _true_input_mag(
        c, 1.0, 4.7e-10, 0.0, 1000.0
    )
    dark = _true_input_mag(c, 1.0, 4.7e-10, 1.0, 6000.0) / _true_input_mag(
        c, 1.0, 4.7e-10, 1.0, 1000.0
    )
    assert dark < bright
