# Pull Request Review

How to review a PR, whether the reviewer is a human or an AI agent. The goal is feedback that improves the change without slowing it down.

## What to Evaluate

- **Correctness:** Does the change do what it claims? Are edge cases, error paths, and concurrency handled? Watch for off-by-one, null, and boundary bugs.
- **Real-time safety:** No heap allocation, locks, exceptions, or I/O in `Node::process()` / `reset()`. Allocation belongs in `prepare()`. New audio-thread code should be covered by the `rt_safety` test.
- **Engine isolation:** The `engine/` and `nodes/` trees must not include a plugin/GUI SDK header (`juce`, `vst`/`pluginterfaces`, `clap`, `AudioUnit`). The `engine_sdk_isolation` test enforces this — do not work around it.
- **Scope:** Is the PR atomic — a single conceptual change? Flag unrelated bundled changes and oversized diffs that should be split. Prefer cohesive vertical slices over fragmentation that obscures intent.
- **Tests:** Are new behaviors covered? Do tests assert outcomes (frequency response, null/A-B equivalence, no-allocation) rather than implementation details? Are negative and edge cases covered?
- **Security:** Apply [security.md](security.md) — no hardcoded secrets, no absolute paths, no leaked internal hostnames, validated input at boundaries.
- **Style & typing:** Spot-check only; trust `clang-format`, `clang-tidy`, and the warnings-as-errors build for the rest. Do not re-flag what tooling already enforces.
- **Documentation:** READMEs, in-code comments, and `CLAUDE.md` updated in the same change when behavior, commands, or layout shift.

## What NOT to Flag

- **Taste-level nits:** Naming, line breaks, comment phrasing — unless they materially obscure intent. Reviewer preference loses to [code-style.md](code-style.md).
- **Lint-fixable items:** Trust tooling; do not duplicate it in review.
- **Speculative future concerns:** "What if we later add X?" — out of scope unless the current change blocks that path.

## Feedback Tone

Follow [communication.md](communication.md): terse, direct, substance first.

- **Cite location:** File and line, not vague references.
- **One concern per comment.** Do not bundle unrelated points.
- **Concrete suggestion:** Where possible, propose the fix, not only the problem.
- **Severity tags optional:** `blocker`, `concern`, `nit`. Default is `concern`. Reserve `blocker` for correctness, real-time-safety, or security.
- **Praise sparingly.** A short note on a non-obvious good decision is useful; reflexive praise on every PR is noise.

## When to Approve

Approve when the change is correct, scoped, and tested — even if minor nits remain. Do not block on items the author can address in a follow-up unless they materially affect correctness, real-time safety, or security.

## Delivery

When reviewing via an automated workflow, deliver feedback as:
- **Inline comments** at specific file:line locations, one per concern.
- **One brief top-level summary comment** with the overall assessment.

Human reviewers can mirror this structure or use whatever the platform supports.
