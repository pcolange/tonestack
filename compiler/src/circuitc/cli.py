"""``circuitc`` command line: compile circuits to generated headers.

The committed golden IR under ``contract/golden`` is the regression anchor: it is rewritten
only on an explicit ``--update-golden`` (or ``--golden-dir``), never as a side effect of a
build. Output paths default to repo-relative locations (no machine-specific absolutes).
"""

from __future__ import annotations

import argparse
from collections.abc import Callable
from pathlib import Path

from .emit_cpp import emit_header
from .ir import CircuitIR, iter_biquads, iter_iir_tables
from .sample import DEFAULT_ROWS, build_rangemaster, build_ts9

DEFAULT_OUT_DIR = Path("nodes/generated")
DEFAULT_GOLDEN_DIR = Path("contract/golden")

CIRCUITS: dict[str, Callable[[int], CircuitIR]] = {
    "ts9": lambda rows: build_ts9(rows=rows),
    "rangemaster": lambda rows: build_rangemaster(),  # grid densities are circuit constants
}


def _repo_root() -> Path:
    """Repo root for an editable install; fall back to cwd for any other layout."""
    # .../compiler/src/circuitc/cli.py -> repo root is parents[3]
    root = Path(__file__).resolve().parents[3]
    if (root / "CMakeLists.txt").exists():
        return root
    return Path.cwd()


def compile_circuit(ir: CircuitIR, out_dir: Path, golden_dir: Path | None) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    header_path = out_dir / f"{ir.name}_tables.h"
    header_path.write_text(emit_header(ir, namespace=ir.name))

    if golden_dir is not None:
        golden_dir.mkdir(parents=True, exist_ok=True)
        for table in iter_biquads(ir.stages):
            (golden_dir / f"{ir.name}_{table.id}.coeffs.json").write_text(
                table.model_dump_json(indent=2) + "\n"
            )
        for iir in iter_iir_tables(ir.stages):
            (golden_dir / f"{ir.name}_{iir.id}.coeffs.json").write_text(
                iir.model_dump_json(indent=2) + "\n"
            )
        (golden_dir / f"{ir.name}.ir.json").write_text(ir.model_dump_json(indent=2) + "\n")
    return header_path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="circuitc")
    sub = parser.add_subparsers(dest="command", required=True)

    compile_p = sub.add_parser("compile", help="compile circuits to generated headers")
    compile_p.add_argument(
        "--circuit",
        choices=["all", *CIRCUITS],
        default="all",
        help="which circuit to compile",
    )
    compile_p.add_argument(
        "--rows", type=int, default=DEFAULT_ROWS, help="pot-axis table rows (ts9)"
    )
    compile_p.add_argument(
        "--out-dir",
        type=Path,
        default=_repo_root() / DEFAULT_OUT_DIR,
        help="generated C++ header directory",
    )
    compile_p.add_argument(
        "--update-golden",
        action="store_true",
        help="deliberately rewrite the committed golden IR (regression anchor)",
    )
    compile_p.add_argument(
        "--golden-dir",
        type=Path,
        default=None,
        help="golden IR output dir (implies --update-golden)",
    )

    args = parser.parse_args(argv)
    if args.command == "compile":
        golden_dir: Path | None = args.golden_dir
        if golden_dir is None and args.update_golden:
            golden_dir = _repo_root() / DEFAULT_GOLDEN_DIR
        names = list(CIRCUITS) if args.circuit == "all" else [args.circuit]
        for name in names:
            ir = CIRCUITS[name](args.rows)
            header = compile_circuit(ir, args.out_dir, golden_dir)
            print(f"circuitc: compiled '{ir.name}' -> {header}")
        if golden_dir is not None:
            print(f"          golden IR -> {golden_dir}")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
