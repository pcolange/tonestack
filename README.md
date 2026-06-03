# ToneStack

**Hybrid circuit-modeled + neural guitar amp/effects DSP framework.**

ToneStack is an open-core, real-time DSP framework for modeling guitar amplifiers and
effects. It runs a processing graph in which physically circuit-modeled nodes and neural
nodes coexist in one signal chain. Its distinguishing feature is an offline **circuit
compiler**: a circuit netlist (and, eventually, a schematic image) is translated through
symbolic analysis into discretized coefficients that drive real-time C++ engine nodes — no
hand-derivation required.

It grows out of [TubeSchemer](https://github.com/pcolange/TubeSchemer), an analog-modeled
Tube Screamer plugin; that pedal becomes the first analytic module here.

## Status

Phase 1 in progress. The engine core, nodes (`GainNode`, `BiquadNode`), a dev harness, and a
headless test suite build and run; the `circuitc` compiler discretizes the Tube Screamer
filters into coefficient tables that the engine consumes through a generated header. Not yet
usable as a plugin.

## Architecture

Dependencies point downward only; the engine and node libraries never include a plugin or
GUI SDK header (enforced by the `engine_sdk_isolation` test).

```
harness / tests          standalone runtime — no plugin SDK
      │
nodes                    analytic + (later) neural + IR nodes
      │
engine                   Node · Chain · ParameterSet · AudioBlock  (GPL-free core)
```

- **`engine/`** — `Node` (prepare/process/reset), a typed lock-free `ParameterSet`, a linear
  `Chain` (itself a `Node`, so composite modules nest), and a non-owning `AudioBlock`. All
  allocation happens in `prepare()`; `process()` is real-time safe.
- **`nodes/`** — processing blocks: `GainNode`, `BiquadNode` (table-driven biquad). The asinh
  waveshaper, oversampling, the composite Tube Screamer module, neural, and convolution nodes
  follow.
- **`harness/`** — a standalone dev runner (offline + live audio).
- **`compiler/`** — the offline Python circuit compiler `circuitc` (discretizer + coefficient
  tables now; Lcapy → scipy symbolic front-end next).
- **`contract/`** — the IR/data contract shared between compiler and engine (see
  `contract/README.md`).

## Building

Requires CMake ≥ 3.21 and a C++17 compiler.

```sh
cmake --preset dev
cmake --build build/dev
ctest --preset dev
./build/dev/harness/tonestack-harness
```

Working on the Python compiler is optional (the C++ build runs without it):

```sh
python3 -m venv compiler/.venv
compiler/.venv/bin/pip install -e 'compiler[dev]'
( cd compiler && .venv/bin/python -m pytest )
```

## License

Open core, permissive (Apache-2.0 or MIT — to be finalized). Proprietary premium content
lives in a separate private repository that links this engine.
