# Phase 6 Deferred Engineering Work

This document records Phase 6 engineering cleanup that was intentionally deferred until the gameplay slices were stable.

## Phase 6R — Automation test-module extraction

Status: **COMPLETE / UE5.8 FULL 53/53 REGRESSION + SHIPPING EXCLUSION PASSED**.

The previous temporary state placed reflected Automation-only helper UObjects inside the main `SlayTheSpireDemo` Runtime module. That source debt has now been removed and validated.

The reflected helpers are:

- `UPhase6ATestExecutionRecorder`
- `UPhase6ATestRecordAction`
- `UPhase6ATestRecordTrigger`
- `UPhase6ATestEmitTurnEndedAction`
- `UPhase6ATestNestedTrigger`

They now live in the Editor-only test module rather than Runtime.

### Implemented architecture

```text
SlayTheSpireDemo          Runtime
SlayTheSpireDemoTests     Editor-only
        ↓ depends on
SlayTheSpireDemo
```

The Runtime module does not depend on the test module.

Automation-only sources have been moved to:

```text
Source/SlayTheSpireDemoTests/Private/
```

including:

```text
Phase5RegressionTests.cpp
Phase6ARegressionTests.cpp
Phase6AExecutionOrderTests.cpp
Phase6ATestTypes.h
Phase6ATestTypes.cpp
Phase6BRegressionTests.cpp
Phase6CRegressionTests.cpp
```

The test-only reflected classes use `SLAYTHESPIREDEMOTESTS_API`.

`SlayTheSpireDemoTests` is declared as an `Editor` module in the project descriptor and is included by the Editor target only. The normal Game target does not include it.

Runtime source contains no `UPhase6ATest*` reflected helper declarations after the extraction.

### Include/link boundary

The existing Runtime module predates a normal `Public/Private` header split. Phase 6R therefore keeps Runtime-header compatibility paths private to `SlayTheSpireDemoTests`; it does not widen the Runtime module's public include surface merely for Automation.

`FTriggerContext` is exported from Runtime because its existing non-inline constructor/getters are legitimately consumed across the new module boundary. This is a linkage/export change only; trigger gameplay semantics are unchanged.

### Validation gate — PASSED

The owner-only manual workflow is:

```text
.github/workflows/ue-phase6r-tests.yml
```

Its regression job built `SlayTheSpireDemoEditor` with the test module and passed the complete fixed gate:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
----------------
Total      53/53
```

Its Shipping-exclusion job started from a clean checkout and built:

```text
SlayTheSpireDemo Win64 Shipping
```

while confirming Shipping artifacts contain no:

```text
SlayTheSpireDemoTests
Phase6ATest
```

### Acceptance criteria — SATISFIED

```text
Runtime module contains no Phase6ATest* reflected classes               PASS
Runtime module has no dependency on SlayTheSpireDemoTests               PASS
Editor target builds the Editor-only test module                        PASS
Editor Automation discovers all migrated tests                          PASS
Phase 5 + Phase 6 regression gates pass at 53/53                        PASS
Shipping game target builds                                              PASS
Shipping artifacts exclude the test module and reflected test helpers    PASS
```

Phase 6R is complete. The guardrail is now permanent: Automation-only reflected helpers belong in `SlayTheSpireDemoTests`, not Runtime.

## Completed cleanup — trigger trace terminology

The temporary `FTriggerDispatchRecord` compatibility name is no longer part of the intended architecture. Phase 6 terminology is:

```text
FTriggerEligibilityRecord
OutEligibilityTrace
```

An eligibility trace records triggers that passed `CanReact` in deterministic candidate order. It does **not** claim that their Reaction Actions were successfully built, inserted, or executed. Actual execution ordering remains covered separately by execution-level Automation tests.

## Current scheduling

```text
Phase 6A    COMPLETE
Phase 6B    COMPLETE
Phase 6C    COMPLETE / 53-test gate passed
Phase 6R    COMPLETE / 53/53 + Shipping exclusion passed
Phase 6     COMPLETE
Phase 6UI-A NEXT
  UI-A0     NEXT
Phase 7     AFTER Phase 6UI-A
```
