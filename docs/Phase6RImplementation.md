# Phase 6R — Regression Gate and Test Module Extraction

Status: **COMPLETE / UE5.8 FULL 53/53 REGRESSION + SHIPPING EXCLUSION PASSED**.

Phase 6R is an engineering/regression slice. It does not add new gameplay, UI, Relic or card mechanics.

## Goal

Move Automation-only code out of the `SlayTheSpireDemo` Runtime module while preserving every validated Phase 5 / Phase 6 gameplay invariant.

Target architecture:

```text
SlayTheSpireDemo          Runtime
SlayTheSpireDemoTests     Editor-only
        ↓ depends on
SlayTheSpireDemo
```

The Runtime module never depends on the test module.

## Test module extraction

`SlayTheSpireDemoTests` is declared as an `Editor` module and is included by `SlayTheSpireDemoEditorTarget`, not the Game target.

The following Automation sources were moved from:

```text
Source/SlayTheSpireDemo/Tests/
```

to:

```text
Source/SlayTheSpireDemoTests/Private/
```

Moved sources:

```text
Phase5RegressionTests.cpp
Phase6ARegressionTests.cpp
Phase6AExecutionOrderTests.cpp
Phase6ATestTypes.h
Phase6ATestTypes.cpp
Phase6BRegressionTests.cpp
Phase6CRegressionTests.cpp
```

The reflected test helpers now export from the test module through `SLAYTHESPIREDEMOTESTS_API` rather than the Runtime module API macro.

Because the existing Runtime project predates a `Public/Private` header layout, the test module keeps two private compatibility include paths: the Runtime module root for module-root-relative includes used by the reflected helper header, plus the existing Runtime `Actions/` directory as a legacy anchor for migrated tests that still use `../Actions`, `../Status`, `../Cards`, etc. This avoids widening the Runtime module's public include surface solely for Automation.

`FTriggerContext` is now exported with `SLAYTHESPIREDEMO_API`. Its constructor/getters were already Runtime behavior; the export only makes that existing trigger-context API linkable from the separate Editor test module and does not change gameplay semantics.

## Runtime / Shipping boundary

The Runtime module source no longer contains the `UPhase6ATest*` reflected classes.

The normal Game target remains:

```text
SlayTheSpireDemo
```

and does not list `SlayTheSpireDemoTests`.

The Editor target includes both:

```text
SlayTheSpireDemo
SlayTheSpireDemoTests
```

The project descriptor declares `SlayTheSpireDemoTests` as `Type = Editor`, so it is not a Shipping gameplay module.

## Phase 6R workflow

Owner-only manual workflow:

```text
.github/workflows/ue-phase6r-tests.yml
```

It preserves the existing self-hosted security boundary:

```text
workflow_dispatch only
repository owner only
main only
self-hosted Windows x64 ue58
contents: read
```

The first job verifies the source/module boundary, builds `SlayTheSpireDemoEditor`, and reruns the complete fixed regression gate:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
----------------
Total      53/53
```

The second job starts from a clean checkout, builds the normal `SlayTheSpireDemo Win64 Shipping` target, and verifies Shipping build artifacts contain no names associated with:

```text
SlayTheSpireDemoTests
Phase6ATest
```

It also rechecks that Runtime source contains no `UPhase6ATest*` reflected helper.

## Validation result

The UE5.8 Phase 6R workflow completed successfully:

```text
SlayTheSpireDemoEditor builds with SlayTheSpireDemoTests   PASS
Editor Automation discovers exactly 53 Phase5–Phase6C tests PASS
Phase5–Phase6C full regression                              53/53 PASS
Runtime source contains no Phase6ATest reflected helpers     PASS
Runtime Build.cs has no SlayTheSpireDemoTests dependency     PASS
Game target excludes SlayTheSpireDemoTests                   PASS
SlayTheSpireDemo Win64 Shipping build                        PASS
Shipping artifacts exclude SlayTheSpireDemoTests/Phase6ATest PASS
```

Phase 6R is complete. No additional PIE, Blueprint, DataAsset or map validation is required for this engineering-only slice because it introduced no new gameplay semantics and the full regression gate remained green.

## Next phase

The next implementation slice is:

```text
Phase 6UI-A0 — Playable Gameplay Boundary
```

Do not begin Phase 7 directly after Phase 6R; the approved development order places the playable UI slice first.
