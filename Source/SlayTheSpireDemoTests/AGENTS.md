# Automation Test Rules

Applies to `Source/SlayTheSpireDemoTests/**`.

## Purpose and Module Boundary

This Editor-only module validates runtime contracts and regressions. Production runtime code must never depend on it. Keep Automation-only reflected classes here and preserve exclusion from Shipping.

## Test Style

Prefer deterministic, focused contract tests. Validate externally meaningful behavior instead of private implementation trivia where practical.

Do not make tests depend on UObject address, actor discovery order, delegate registration order, unstable container iteration, frame timing or animation timing.

When changing an existing contract:

1. Add or update the smallest focused test.
2. Run the focused suite once.
3. Run an aggregate regression gate only when the applicable phase/acceptance document explicitly requires it, a shared contract change requires it, or a concrete failure invalidates broader evidence.

A passing focused Gate is sticky. Do not rerun it after unrelated edits. If a test fails, fix the failed contract and rerun only the tests invalidated by that fix unless the build or an explicit final-head rule requires more.

Do not create additional synthetic fixtures merely to reconfirm a contract already proven by an existing focused test. Do not use Automation to imitate genuinely visual/manual acceptance when a short manual PIE check is the correct Gate.

Do not add permanent runtime debug/rule APIs merely to make Automation convenient when the invariant can be exercised through existing public/runtime boundaries.

## Automated vs Manual Ownership

Use Automation for deterministic state and protocol assertions such as IDs, counts, enums, delegate calls, exact Tokens, stale callback behavior, Finish/Cancel semantics, timers, frozen DTOs, historical restore, Widget lifecycle state and invalid-payload fallback.

Leave genuinely visual/player-facing criteria to the manual PIE Gate defined by the applicable phase: animation appearance, movement, layout, hover/mouse feel, visible flicker/flashback, duplicate visual instances and Legacy-vs-Native visual parity.

Do not replace a manual PIE Gate with repeated screenshot inspection.

For the repository-wide validation budget, failure/rerun policy, screenshot policy and the Phase6UI-A2N automated/manual split, follow `docs/ValidationExecutionPolicy.md`.

## Validation Claims

Automation PASS does not imply Blueprint graph correctness, UMG visual correctness, manual PIE acceptance or packaged-game acceptance.

Do not claim PIE, build, Shipping exclusion or aggregate regression success unless that exact validation was run. Treat historical totals as evidence, not permanent acceptance constants; do not arithmetically combine suites run under different configured prefixes.

Current trusted evidence is recorded in `docs/Validation.md`.
