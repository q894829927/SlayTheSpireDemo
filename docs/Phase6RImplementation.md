# Phase 6R — Regression Gate and Test Module Extraction

Status: **SOURCE IMPLEMENTED / UE5.8 REGRESSION + SHIPPING EXCLUSION VALIDATION PENDING**.

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

The following Automation sources are moved from:

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

Because the existing Runtime project predates a `Public/Private` header layout, the test module keeps a private legacy include anchor at the existing Runtime `Actions/` directory. The migrated tests retain their established `../Actions`, `../Status`, `../Cards`, etc. include shapes; resolving them through this private anchor avoids widening the Runtime module's public include surface solely for Automation.

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

New owner-only manual workflow:

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

The second job starts from a clean checkout, builds the normal `SlayTheSpireDemo Win64 Shipping` target, and fails if Shipping build artifacts contain names associated with:

```text
SlayTheSpireDemoTests
Phase6ATest
```

It also rechecks that Runtime source contains no `UPhase6ATest*` reflected helper.

## Acceptance criteria

Phase 6R is complete only after the UE5.8 workflow proves all of the following:

```text
SlayTheSpireDemoEditor builds with SlayTheSpireDemoTests
Editor Automation still discovers exactly 53 Phase5–Phase6C tests
all 53 tests pass
Runtime source contains no Phase6ATest reflected helpers
Runtime Build.cs does not depend on SlayTheSpireDemoTests
Game target does not include SlayTheSpireDemoTests
Shipping game target builds
Shipping build artifacts contain no SlayTheSpireDemoTests / Phase6ATest artifacts
```

No Phase 6R validation is claimed until that workflow passes on the UE5.8 self-hosted runner.

## Next phase

After Phase 6R passes and documentation is synchronized, the next implementation slice is:

```text
Phase 6UI-A0 — Playable Gameplay Boundary
```

Do not begin Phase 7 directly after Phase 6R; the approved development order places the playable UI slice first.
