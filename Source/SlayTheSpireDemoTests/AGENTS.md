# Automation Test Rules

Applies to `Source/SlayTheSpireDemoTests/**`.

## Purpose and Module Boundary

This Editor-only module validates runtime contracts and regressions. Production runtime code must never depend on it. Keep Automation-only reflected classes here and preserve exclusion from Shipping.

## Test Style

Prefer deterministic, focused contract tests. Validate externally meaningful behavior instead of private implementation trivia where practical.

Do not make tests depend on UObject address, actor discovery order, delegate registration order, unstable container iteration, frame timing or animation timing.

When changing an existing contract:

1. Add or update the smallest focused test.
2. Run the focused suite.
3. Run the required aggregate regression gate when the focused test passes and the environment permits it.

Do not add permanent runtime debug/rule APIs merely to make Automation convenient when the invariant can be exercised through existing public/runtime boundaries.

## Validation Claims

Automation PASS does not imply Blueprint graph correctness, UMG visual correctness, manual PIE acceptance or packaged-game acceptance.

Do not claim PIE, build, Shipping exclusion or aggregate regression success unless that exact validation was run. Treat historical totals as evidence, not permanent acceptance constants; do not arithmetically combine suites run under different configured prefixes.

Current trusted evidence is recorded in `docs/Validation.md`.
