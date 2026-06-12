"""Codegen: turn a compiled IR into a POD C++ header of ``constexpr`` coefficient tables.

The emitted header is the only thing the real-time engine sees of the compiler — no JSON is
parsed on the audio path. Output lands in ``nodes/generated/`` (a build artifact, gitignored)
and is consumed by ``BiquadNode`` / the Tube Screamer module.
"""

from __future__ import annotations

from .ir import (
    BiquadParamTable,
    BjtWaveshaper,
    CircuitIR,
    IirParamTable,
    Oversample,
    ParamSpec,
    Skew,
    Waveshaper,
    iter_biquads,
    iter_bjt_waveshapers,
    iter_iir_tables,
    iter_waveshapers,
)


def _f(x: float) -> str:
    s = f"{x:.9g}"
    # A float literal needs a '.' or exponent; "0", "51000", "1" would be ill-formed with 'f'.
    if not any(c in s for c in ".eEnN"):
        s += ".0"
    return s + "f"


def _rate_token(rate: float) -> str:
    return str(int(round(rate)))


def _skew_token(skew: Skew) -> str:
    return "ParamSkew::Logarithmic" if skew is Skew.logarithmic else "ParamSkew::Linear"


def _param_literal(p: ParamSpec) -> str:
    return (
        f'ParameterDesc{{"{p.id}", {_f(p.min)}, {_f(p.max)}, {_f(p.default_proportion)}, '
        f"{_skew_token(p.skew)}, {_f(p.skew_midpoint)}, {_f(p.smoothing_seconds)}}}"
    )


def _emit_params(table_id: str, params: list[ParamSpec], out: list[str]) -> None:
    out.append(f"inline ParameterDesc {table_id}Param(int axis = 0) {{")
    out.append("    switch (axis) {")
    for k, p in enumerate(params):
        out.append(f"    case {k}: return {_param_literal(p)};")
    out.append("    default: return {};")
    out.append("    }")
    out.append("}")


def _emit_biquad(table: BiquadParamTable, out: list[str]) -> None:
    tid = table.id
    dims = [len(axis) for axis in table.axes]
    num_axes = len(dims)
    total = 1
    for d in dims:
        total *= d
    out.append(f"// --- {tid} biquad ({num_axes} axis/axes, {total} grid points) ---")
    out.append(f"inline constexpr int {tid}_num_axes = {num_axes};")
    for k, axis in enumerate(table.axes):
        vals = ", ".join(_f(v) for v in axis)
        out.append(f"inline constexpr float {tid}_axis{k}[{dims[k]}] = {{{vals}}};")
    axes_list = ", ".join(f"{tid}_axis{k}" for k in range(num_axes))
    out.append(f"inline constexpr const float* {tid}_axes[{num_axes}] = {{{axes_list}}};")
    dims_list = ", ".join(str(d) for d in dims)
    out.append(f"inline constexpr int {tid}_dims[{num_axes}] = {{{dims_list}}};")
    for rate in table.rates:
        tok = _rate_token(rate.sample_rate)
        out.append(f"inline constexpr BiquadSection {tid}_sections_{tok}[{total}] = {{")
        for s in rate.sections:
            out.append(f"    {{{_f(s.b0)}, {_f(s.b1)}, {_f(s.b2)}, {_f(s.a1)}, {_f(s.a2)}}},")
        out.append("};")
        out.append(
            f"inline constexpr BiquadCoeffTable {tid}_table_{tok}"
            f"{{{tid}_axes, {tid}_dims, {num_axes}, "
            f"{tid}_sections_{tok}, {rate.sample_rate:.1f}}};"
        )
    out.append(f"inline const BiquadCoeffTable* {tid}Table(double sampleRate) noexcept {{")
    out.append("    const long long hz = static_cast<long long>(sampleRate + 0.5);")
    for rate in table.rates:
        tok = _rate_token(rate.sample_rate)
        out.append(f"    if (hz == {tok}) return &{tid}_table_{tok};")
    out.append("    return nullptr;")
    out.append("}")
    _emit_params(tid, table.params, out)


def _emit_iir(table: IirParamTable, out: list[str]) -> None:
    tid = table.id
    dims = [len(axis) for axis in table.axes]
    num_axes = len(dims)
    total = 1
    for d in dims:
        total *= d
    width = 2 * table.order + 1
    out.append(f"// --- {tid} IIR order {table.order} ({num_axes} axes, {total} grid points) ---")
    out.append(f"inline constexpr int {tid}_order = {table.order};")
    out.append(f"inline constexpr int {tid}_num_axes = {num_axes};")
    for k, axis in enumerate(table.axes):
        vals = ", ".join(_f(v) for v in axis)
        out.append(f"inline constexpr float {tid}_axis{k}[{dims[k]}] = {{{vals}}};")
    axes_list = ", ".join(f"{tid}_axis{k}" for k in range(num_axes))
    out.append(f"inline constexpr const float* {tid}_axes[{num_axes}] = {{{axes_list}}};")
    dims_list = ", ".join(str(d) for d in dims)
    out.append(f"inline constexpr int {tid}_dims[{num_axes}] = {{{dims_list}}};")
    for rate in table.rates:
        tok = _rate_token(rate.sample_rate)
        out.append(f"inline constexpr float {tid}_coeffs_{tok}[{total * width}] = {{")
        for row in rate.rows:
            out.append("    " + ", ".join(_f(v) for v in row) + ",")
        out.append("};")
        out.append(
            f"inline constexpr IirCoeffTable {tid}_table_{tok}"
            f"{{{tid}_axes, {tid}_dims, {num_axes}, "
            f"{tid}_coeffs_{tok}, {table.order}, {rate.sample_rate:.1f}}};"
        )
    out.append(f"inline const IirCoeffTable* {tid}Table(double sampleRate) noexcept {{")
    out.append("    const long long hz = static_cast<long long>(sampleRate + 0.5);")
    for rate in table.rates:
        tok = _rate_token(rate.sample_rate)
        out.append(f"    if (hz == {tok}) return &{tid}_table_{tok};")
    out.append("    return nullptr;")
    out.append("}")
    _emit_params(tid, table.params, out)


def _emit_waveshaper(s: Waveshaper, out: list[str]) -> None:
    out.append(f"// --- {s.id} waveshaper ({s.kind}); control param: '{s.r2_param}' ---")
    out.append(
        f"inline constexpr AsinhDiodeShape {s.id}_shape{{"
        f"{_f(s.saturation_current)}, {_f(s.thermal_voltage)}, "
        f"{_f(s.r2_const)}, {_f(s.r2_pot)}}};"
    )


def _emit_bjt_waveshaper(s: BjtWaveshaper, out: list[str]) -> None:
    out.append(f"// --- {s.id} waveshaper ({s.kind}) ---")
    out.append(
        f"inline constexpr GeBjtShape {s.id}_shape{{"
        f"{_f(s.thermal_voltage)}, {_f(s.gain_volts)}, "
        f"{_f(s.headroom)}, {_f(s.saturation_input)}}};"
    )


def emit_header(ir: CircuitIR, namespace: str = "ts9") -> str:
    biquads = list(iter_biquads(ir.stages))
    shapers = list(iter_waveshapers(ir.stages))
    out: list[str] = [
        "#pragma once",
        "",
        f"// Generated by circuitc from circuit '{ir.name}' (ir_version {ir.ir_version}).",
        "// Do not edit: regenerate with `circuitc compile`.",
        "",
        '#include "tonestack/Parameter.h"',
        '#include "tonestack/nodes/BiquadCoeffs.h"',
        '#include "tonestack/nodes/IirCoeffs.h"',
        '#include "tonestack/nodes/WaveshaperShapes.h"',
        "",
        f"namespace tonestack::nodes::generated::{namespace} {{",
        "",
        f"inline constexpr int oversample_factor = "
        f"{next((s.factor for s in ir.stages if isinstance(s, Oversample)), 1)};",
        "",
    ]
    for table in biquads:
        _emit_biquad(table, out)
        out.append("")
    for iir in iter_iir_tables(ir.stages):
        _emit_iir(iir, out)
        out.append("")
    for shaper in shapers:
        _emit_waveshaper(shaper, out)
        out.append("")
    for bjt in iter_bjt_waveshapers(ir.stages):
        _emit_bjt_waveshaper(bjt, out)
        out.append("")
    out.append(f"}} // namespace tonestack::nodes::generated::{namespace}")
    out.append("")
    return "\n".join(out)
