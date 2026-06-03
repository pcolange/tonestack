---
paths:
  - "**/*.h"
  - "**/*.hpp"
  - "**/*.cpp"
  - "**/*.cc"
---

# Code Style

Write clean, modern, DRY C++17. Follow these rules when writing or modifying C++ in this repo.
The offline `compiler/` subtree is Python and defers to its own `ruff` / `pyright` config in
`compiler/pyproject.toml` — these rules govern the C++ engine, nodes, harness, and tests.

## Linting & Formatting

- Follow the project's `clang-format` / `clang-tidy` configuration if present.
- The build is warnings-clean under `-Wall -Wextra -Wpedantic -Wshadow -Wconversion
  -Wsign-conversion`; CI builds with warnings-as-errors (`TONESTACK_WARNINGS_AS_ERRORS=ON`).
  New code must not introduce warnings.

## Real-Time Safety (critical)

- `prepare()` is the **only** place a `Node` may allocate. `process()` and `reset()` must be
  real-time safe: no heap allocation, no locks, no exceptions, no syscalls, no unbounded work.
- Mark `process()` / `reset()` `noexcept`.
- No `new` / `make_unique` / container resize / `std::string` work / I/O inside `process()`.
- The `rt_safety` test asserts `process()` performs zero heap allocation — keep it passing.

## Conventions

- C++17. Prefer RAII and value semantics; `std::unique_ptr` for ownership, raw pointers/refs
  for non-owning views only.
- Naming: `PascalCase` types, `camelCase` functions/methods/locals, private members carry a
  trailing underscore (`member_`), namespaces are `lower_snake` (`tonestack`,
  `tonestack::nodes`). Use `enum class`.
- `#pragma once`; headers are self-contained and include what they use.
- Mark overrides `override`; be `const`-correct.
- The engine and node libraries must **never** include a plugin/GUI SDK header
  (`juce`, `vst`/`pluginterfaces`, `clap`, `AudioUnit`) — enforced by the
  `engine_sdk_isolation` test.
- YAGNI: no speculative abstractions. DRY: shared helpers over copy-paste.

## Git Commit Messages

- **Single line only.** A concise, imperative summary (e.g. `Add BiquadNode and tone-stack
  coefficient table`). No body paragraphs.
- **No co-author or trailer text.** Do not append `Co-Authored-By` or any "generated with"
  footer.

## Documentation & Maintenance

- Any change to functionality, commands, build steps, or layout must be reflected in the
  README / `CLAUDE.md` in the same change.
- In-code comments: sparing and brief; favor well-named identifiers over restating the code.
  Do not narrate implementation history ("added for X", "TODO revisit", "previously did Y").
  Exceptions worth a comment: a hidden constraint a reader cannot infer, and real-time-safety
  contract notes.
- Update comments in the same commit as the code change.

## Module Boundaries

- Dependencies point downward only: `plugin` / `harness` / `tests` → `nodes` → `engine` →
  vendored DSP. The engine core stays plugin-SDK-agnostic and GPL-free so proprietary modules
  can link it.
