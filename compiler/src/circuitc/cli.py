"""``circuitc`` command line: compile the Tube Screamer to a generated header + golden IR.

Output paths default to repo-relative locations (no machine-specific absolutes), resolved from
this file's position in the source tree.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from .emit_cpp import emit_header
from .ir import CircuitIR, iter_biquads
from .sample import DEFAULT_ROWS, build_ts9

# .../compiler/src/circuitc/cli.py -> repo root is parents[3]
REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_HEADER = Path("nodes/generated/ts9_tables.h")
DEFAULT_GOLDEN_DIR = Path("contract/golden")


def compile_ts9(rows: int, header_path: Path, golden_dir: Path) -> CircuitIR:
    ir = build_ts9(rows=rows)

    header_path.parent.mkdir(parents=True, exist_ok=True)
    header_path.write_text(emit_header(ir, namespace=ir.name))

    golden_dir.mkdir(parents=True, exist_ok=True)
    for table in iter_biquads(ir.stages):
        (golden_dir / f"{ir.name}_{table.id}.coeffs.json").write_text(
            table.model_dump_json(indent=2) + "\n"
        )
    (golden_dir / f"{ir.name}.ir.json").write_text(ir.model_dump_json(indent=2) + "\n")
    return ir


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="circuitc")
    sub = parser.add_subparsers(dest="command", required=True)

    compile_p = sub.add_parser("compile", help="compile the TS9 circuit to header + golden IR")
    compile_p.add_argument("--rows", type=int, default=DEFAULT_ROWS, help="pot-axis table rows")
    compile_p.add_argument(
        "--out-header", type=Path, default=REPO_ROOT / DEFAULT_HEADER, help="generated C++ header"
    )
    compile_p.add_argument(
        "--golden-dir",
        type=Path,
        default=REPO_ROOT / DEFAULT_GOLDEN_DIR,
        help="golden IR output dir",
    )

    args = parser.parse_args(argv)
    if args.command == "compile":
        ir = compile_ts9(args.rows, args.out_header, args.golden_dir)
        print(f"circuitc: compiled '{ir.name}' -> {args.out_header}")
        print(f"          golden IR -> {args.golden_dir}")
        return 0
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
