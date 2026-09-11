# Selection Presentation G8-A Execution

Date: 2026-09-11

Status: **IMPLEMENTED / BUILD PASS / FOCUSED AUTOMATION PASS / AFFECTED REGRESSION PASS**.

G8-A implements the infrastructure stage defined by `docs/SelectionPresentationG8Design.md`. Production Damage remains on the existing Blocking Presentation path; detached DamageNumber production behavior, compatibility debt, early-input readiness, and the later G8-B/G8-C contracts are not enabled by this stage.

## Implemented scope

- Extracted one authoritative historical Damage reducer/validator and reused it from the existing Blocking Controller reduction path.
- Added Presentation session identity using `BattleId + ControllerEpoch + PresentationSessionGeneration`.
- Ordinary Skip and active-envelope reconcile retain the current Presentation session.
- HUD/Controller binding replacement invalidates the old session, retires old playback ownership/generation, installs the new Widget, mints the replacement session before catch-up, and prevents old playback tokens from being sent to the new Widget.
- Controller initialization failure now uses the PresentationUnavailable fail-safe instead of silently falling back to DirectBaseline.
- Added production-disabled detached Damage infrastructure: transient VFX host, exact detached token/spec, prepared/active lifecycle, GC-owned instance array, NativeTick finite-duration cleanup, exact/session cleanup, instance ceiling, and presentation-only Damage combatant cue entry point.
- Production `Damage` playback remains Blocking in G8-A.

Implementation was merged directly to `main`; the implementation checkpoint before validation was:

```text
ac39e4267774278f2cc6a8f9699aabbd371830b1
```

## Validation evidence

### Development Editor build

User reported the prescribed UE 5.8 Development Editor compilation completed successfully on 2026-09-11.

Result:

```text
PASS
```

### G8-A focused Automation

Command scope:

```text
SlayTheSpireDemo.SelectionPresentation.G8A
```

Report path:

```text
Saved/AutomationReports/G8A
```

Result from Unreal Automation log:

```text
Automation Test Queue Empty 4 tests performed
process TestExit status: 0
```

Passing tests:

```text
SlayTheSpireDemo.SelectionPresentation.G8A.DamageReducer
SlayTheSpireDemo.SelectionPresentation.G8A.InvalidDamagePreflight
SlayTheSpireDemo.SelectionPresentation.G8A.SessionContinuity
SlayTheSpireDemo.SelectionPresentation.G8A.WidgetReplacementOwnership
```

Result:

```text
4/4 PASS
```

The `DebugStartingDeck is empty` messages are expected warnings from minimal test fixtures with an intentionally empty deck; no test failure was reported.

### Affected stale/replacement regression

Command scope:

```text
SlayTheSpireDemo.Phase6UIA2A.Hardening.ControllerStaleIsolation
```

Report path:

```text
Saved/AutomationReports/G8ARegression
```

Result from Unreal Automation log:

```text
Automation Test Queue Empty 1 tests performed
process TestExit status: 0
```

Passing test:

```text
SlayTheSpireDemo.Phase6UIA2A.Hardening.ControllerStaleIsolation
```

Result:

```text
1/1 PASS
```

The PresentationUnavailable messages in this test are intentional fault-injection coverage for Presentation seal/append failure. The test completed with `Result={Success}`.

## Acceptance

G8-A automated acceptance is satisfied:

```text
[x] UE 5.8 Development Editor build PASS
[x] single Damage reducer focused coverage PASS
[x] invalid Damage rejected before Widget playback PASS
[x] ordinary Skip/reconcile session continuity PASS
[x] Controller replacement ABA protection PASS
[x] Widget replacement old-token ownership isolation PASS
[x] affected Controller stale/replacement regression PASS
[x] production detached Damage remains disabled
```

No dedicated visual PIE gate is required for G8-A because detached DamageNumber production behavior is not enabled in this stage. Future G8-C/G8-D visual behavior must not infer visual acceptance from these headless tests.

## Forward boundary

G8-A is complete and validated as an infrastructure stage. Do not infer completion of later G8 stages.

Next stage, only when separately authorized:

```text
G8-B — readiness / exact PendingSelection identity / FastInput shadow contracts
```
