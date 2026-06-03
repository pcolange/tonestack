# CLAUDE.md

Guidance for Claude Code working in this repository.

## What this is

**ToneStack** is an open-core, real-time DSP framework for modeling guitar amplifiers and
effects. It runs a processing **graph** in which physically circuit-modeled (analytic) nodes
and neural nodes coexist in one signal chain. Its distinguishing feature is an offline
**circuit compiler**: a circuit netlist (and eventually a schematic image) is translated by
symbolic analysis into discretized coefficients that drive real-time C++ engine nodes — no
hand-derivation.

It grows out of the archived [TubeSchemer](https://github.com/pcolange/TubeSchemer) Tube
Screamer plugin; that pedal's circuit-derived DSP is the first analytic module and the golden
reference for the compiler.

The full design and phased roadmap live in the plan file:
`~/.claude-personal/plans/ok-given-what-we-ve-sunny-rivest.md`.

## Status

**Phase 0 complete.** Engine core, first node (`GainNode`), dev harness, headless tests, and CI
build and pass. Not yet a plugin.

**Next — Phase 1 (two tracks):**
1. Port the Tube Screamer to engine nodes: `BiquadNode`, `WaveshaperNode` (asinh diode),
   `OversampledNode`, composite `TubeScreamerModule`.
2. Stand up the Python compiler `circuitc` (`compiler/`): Lcapy MNA → scipy `bilinear_zpk` →
   coefficient-table IR → generated C++ header, with a golden regression that reproduces the
   legacy `updateFilterState()` coefficients to ~1e-9.

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
  `ParameterSet` (log-skew + per-block smoothing; mirrors the original JUCE `NormalisableRange`
  taper), a linear `Chain` (itself a `Node`, so composite modules nest), and a non-owning
  `AudioBlock`. **All allocation in `prepare()`; `process()`/`reset()` are real-time safe.**
- **`nodes/`** — processing blocks. Currently `GainNode` (header-only INTERFACE lib).
- **`harness/`** — standalone dev runner (synthetic signal now; WAV + live audio later).
- **`compiler/`** — the offline Python circuit compiler (not yet implemented).
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
