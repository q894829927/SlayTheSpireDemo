# Automation Test Rules

Applies to `Source/SlayTheSpireDemoTests/**`. Module isolation and Shipping exclusion are defined in `Source/AGENTS.md`.

## Test contracts

Use focused, deterministic tests of externally meaningful behavior. Keep ordering independent of UObject address, actor discovery, delegate registration, unstable iteration, frames and animations.

Add or update coverage when a changed contract is not adequately proven by existing tests. Exercise existing public/runtime boundaries; do not add permanent runtime debug/rule APIs just for test convenience. Do not create duplicate synthetic fixtures for already proven behavior or use Automation to imitate visual acceptance.

## Execution and claims

Follow `docs/ValidationExecutionPolicy.md` for build/test scope, evidence reuse, failure reruns and automated/manual ownership. C++ Automation PASS does not imply Blueprint graph correctness, UMG visual correctness, PIE or packaged-game acceptance.

Record actual evidence in `docs/Validation.md` or the applicable dedicated validation document. Treat historical totals as evidence, not permanent acceptance constants; do not combine suites run under different configured prefixes.
