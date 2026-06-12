# CLAUDE.md

Guidance for Claude Code working in this repository.

## What this is

**ToneStack** is an open-core, real-time DSP framework for modeling guitar amplifiers and
effects. It runs a processing **graph** in which physically circuit-modeled (analytic) nodes
and neural nodes coexist in one signal chain. Its distinguishing feature is an offline
**circuit compiler**: a circuit netlist (and eventually a schematic image) is translated by
symbolic analysis into discretized coefficients that drive real-time C++ engine nodes — no
hand-derivation.

The Tube Screamer is its first analytic module. The project originated as the archived
[TubeSchemer](https://github.com/pcolange/TubeSchemer) plugin, but the implementation here is
its own — TubeSchemer is reference material, not a spec to match.

The full design and phased roadmap live in the plan file:
`~/.claude-personal/plans/ok-given-what-we-ve-sunny-rivest.md`.

## Status

**Phase 1 well advanced** (two circuits compile end-to-end; one is audible):

- **Engine/nodes.** `BiquadNode` (multilinear N-axis grid interpolation), `IirNode`
  (direct-form low-order grids for networks whose factored form can't be interpolated
  continuously), `WaveshaperNode<Shape>` (asinh diode, germanium BJT shapes),
  `OversampledNode` (×4, halfband FIR pairs, exact 29-sample latency), and
  `RangemasterModule` — the first composite module (constructed per sample rate; the
  plugin-shell phase adds rate-flexible construction + parameter flattening).
  `TubeScreamerModule` is next and reuses all of the above.
- **`circuitc` compiler.** TS9 (closed-form discretizer) and Rangemaster (numeric
  polynomial-MNA, DC bias solver in `bias.py`) compile to headers + committed goldens.
  IR 0.3.0: multi-axis `BiquadParamTable`, `IirParamTable`, two waveshaper stages,
  `Oversample`. The symbolic **Lcapy MNA front-end** (must reproduce the same goldens) is
  next. Ground truth per circuit lives in `compiler/circuits/*-reference.md` /
  `REFERENCE.md` — fidelity to the factory schematics is the goal.
- **Meeting point proven both ways.** `circuitc compile` emits `ts9_tables.h` +
  `rangemaster_tables.h`; C++ golden tests drive `BiquadNode`/`IirNode`/the module from
  them, and the harness demos the Rangemaster's treble boost + guitar-volume cleanup.
  CMake codegen is gated on the compiler venv (a C++-only build skips it).

**Phase 0 complete.** Engine core, `GainNode`, dev harness, headless tests, and CI build/pass.
Not yet a plugin.

## Architecture

Dependencies point **downward only**; the engine and node libraries never include a plugin or
GUI SDK header (enforced by the `engine_sdk_isolation` CTest).

```
harness / tests / (future plugin shell)   standalone runtime
      │
nodes        analytic + (later) neural + IR nodes        ← permissive open core
      │
engine       Node · Chain · ParameterSet · AudioBlock    ← GPL-free core, no plugin/GUI deps
      │
vendored DSP (RTNeural, chowdsp_* — added in later phases)
```

- **`engine/`** — `Node` (`prepare`/`process`/`reset`/`parameters`/`info`), a typed lock-free
  `ParameterSet` (log-skew + per-block smoothing), a linear `Chain` (itself a `Node`, so
  composite modules nest), and a non-owning
  `AudioBlock`. **All allocation in `prepare()`; `process()`/`reset()` are real-time safe.**
- **`nodes/`** — processing blocks (header-only INTERFACE lib): `GainNode`, `BiquadNode`,
  `IirNode`, `WaveshaperNode`, `OversampledNode`, `RangemasterModule`, and the POD contract
  types (`BiquadCoeffs.h`, `IirCoeffs.h`, `WaveshaperShapes.h`). `nodes/generated/` holds
  codegen output from `circuitc` (gitignored build artifact); only codegen-gated targets
  may include it.
- **`harness/`** — standalone dev runner (synthetic signal now; WAV + live audio later).
- **`compiler/`** — the offline Python circuit compiler `circuitc` (`src/circuitc/`):
  `ir.py` (pydantic IR, contract source of truth), `netlist.py` (component values),
  `bias.py` (Rangemaster DC operating point), `discretize.py` (TS9 closed forms +
  Rangemaster polynomial-MNA → bilinear), `sample.py` (axis sweeps → IR), `emit_cpp.py`
  (IR → `constexpr` headers), `cli.py` (circuit registry). Tests + tooling run in
  `compiler/.venv` (ruff, pyright strict, pytest). `circuits/*.net` are the netlist sources
  for the future Lcapy front-end; `circuits/REFERENCE.md` (TS9) and
  `circuits/rangemaster-reference.md` are the schematic-traced ground truths (component
  values, pot tapers, transistor/diode constants, modeled-vs-omitted stages) — fidelity to
  the factory circuits is the goal, and changes to circuit values must be justified against
  them.
- **`contract/`** — the IR/data contract shared between compiler and engine (pydantic schema is
  the single source of truth; coefficient **tables** sampled across pot positions, codegen'd to
  a POD C++ header — no JSON or parametric eval in the RT path).
- **`cmake/`** — `CompilerWarnings.cmake`, `EngineGuard.cmake` + `EngineGuardScan.cmake` (the
  SDK-isolation gate).

## Building

Requires CMake ≥ 3.21 and a C++17 compiler. No ninja/clang assumed; Makefiles are fine.

```sh
cmake --preset dev          # or: ci  (Release + warnings-as-errors)
cmake --build build/dev -j
ctest --preset dev
./build/dev/harness/tonestack-harness
```

The Python compiler is optional for a C++ build (codegen is gated on the venv). To work on it:

```sh
python3 -m venv compiler/.venv
compiler/.venv/bin/pip install -e 'compiler[dev]'
compiler/.venv/bin/circuitc compile                  # regen header only
compiler/.venv/bin/circuitc compile --update-golden  # also rewrite committed golden IR
( cd compiler && .venv/bin/python -m pytest && .venv/bin/ruff check . && .venv/bin/pyright )
```

`circuitc compile` writes `nodes/generated/ts9_tables.h` (build artifact). The committed
golden IR under `contract/golden/` is a regression anchor: it is rewritten only by a
deliberate `--update-golden`, never as a build side effect. The CMake codegen step
regenerates the header automatically when the venv is present.

## Conventions

- **Code style:** see `.claude/rules/code-style.md`. C++17, `PascalCase` types, `camelCase`
  functions, `member_` trailing underscore, `lower_snake` namespaces. Build is warnings-clean
  under `-Wconversion -Wsign-conversion`; CI is warnings-as-errors.
- **Real-time safety is a hard rule:** no allocation/locks/exceptions in `process()`. The
  `rt_safety` test guards it.
- **Engine SDK isolation is a hard rule:** no `juce`/`vst`/`clap`/`AudioUnit` headers under
  `engine/` or `nodes/`. The `engine_sdk_isolation` test guards it.
- **Git commit messages: single line, no co-author/trailer text.** (See code-style.md.)
- **Licensing posture:** open core stays permissive and GPL-free so a separate private repo of
  proprietary nodes/models can link it. Keep any GPL/VST3 dependency out of `engine`/`nodes`.

## Git / GitHub

- Owner is the **pcolange** account. The default `github.com` SSH host is pinned to a different
  (work) key, so this repo's remote uses the alias: `git@github-personal:pcolange/tonestack.git`.
- `gh` CLI is authenticated as a different user with only READ on pcolange repos — admin API
  ops (archiving, repo creation under pcolange) must be done via the GitHub web UI.
