# Phase 6 Deferred Engineering Work

This document records accepted engineering cleanup that should not interrupt the Phase 6B / 6C gameplay slices.

## Deferred to Phase 6R — move Automation test UObjects out of the Runtime module

### Current state

`Source/SlayTheSpireDemo/Tests/Phase6ATestTypes.h/.cpp` currently defines reflected test-only UObject types inside the main `SlayTheSpireDemo` Runtime module:

- `UPhase6ATestExecutionRecorder`
- `UPhase6ATestRecordAction`
- `UPhase6ATestRecordTrigger`
- `UPhase6ATestEmitTurnEndedAction`
- `UPhase6ATestNestedTrigger`

`UCLASS(Transient)` does not make a reflected class editor/test-only. Because these types belong to the Runtime module, they remain part of that module's reflected/generated code and can be carried by non-editor builds.

This is accepted temporarily for the learning project because it does not change Phase 6A gameplay semantics or the verified Automation behavior, but it is technical debt and must not become the pattern for future test support.

### Guardrail until the cleanup is done

Do **not** add new test-only `UCLASS` types to the `SlayTheSpireDemo` Runtime module.

If Phase 6B / 6C tests need additional reflected test helpers, prefer reusing the existing Phase 6A test types when that remains semantically clear. If genuinely new reflected helpers are required, perform the test-module extraction first instead of expanding Runtime test infrastructure.

Do not use a casual `#if WITH_DEV_AUTOMATION_TESTS` wrapper around reflected `UCLASS` declarations as a substitute for module separation; UHT/generated-code behavior must stay structurally valid across targets.

### Phase 6R target architecture

Create a separate editor/developer-only test module, tentatively:

```text
SlayTheSpireDemo          Runtime
SlayTheSpireDemoTests     Editor-only test module
        ↓ depends on
SlayTheSpireDemo
```

Move Automation-only sources out of the Runtime module, including at minimum:

```text
Phase5RegressionTests.cpp
Phase6ARegressionTests.cpp
Phase6AExecutionOrderTests.cpp
Phase6ATestTypes.h
Phase6ATestTypes.cpp
```

Move later Phase 6B / 6C Automation sources there as part of the same cleanup.

The Runtime module must never depend on the test module.

### Required migration work

1. Add the test module and its `Build.cs` / project module declaration using an editor/developer-only module type appropriate for UE5.8 Automation.
2. Make the test module depend on `SlayTheSpireDemo`, never the reverse.
3. Move Automation sources and reflected test helper types into the test module.
4. Replace `SLAYTHESPIREDEMO_API` on test-only reflected types with the test-module API macro if export is required.
5. Update include paths without weakening Runtime encapsulation solely for tests.
6. Keep the existing owner-only manual self-hosted UE5.8 CI gate working after the module split.
7. Add a packaging/Shipping-oriented check demonstrating that Phase 6 test-only reflected classes are not part of the Runtime/Shipping product.

### Acceptance criteria

The cleanup is complete only when all of the following are true:

```text
Runtime module contains no Phase6ATest* reflected classes
Runtime module has no dependency on SlayTheSpireDemoTests
Editor Automation discovers the migrated tests
Phase 5 + Phase 6 regression gates still pass
Shipping/package-oriented validation does not contain the test-only reflected types
```

## Completed cleanup — trigger trace terminology

The temporary `FTriggerDispatchRecord` compatibility name is no longer part of the intended architecture. Phase 6 terminology is:

```text
FTriggerEligibilityRecord
OutEligibilityTrace
```

An eligibility trace records triggers that passed `CanReact` in deterministic candidate order. It does **not** claim that their Reaction Actions were successfully built, inserted, or executed. Actual execution ordering remains covered separately by execution-level Automation tests.

## Scheduling

Current order remains:

```text
Phase 6A  COMPLETE / UE5.8 Automation passed
Phase 6B  Battle Turn Wiring
Phase 6C  DeckShuffled
Phase 6R  Full regression + test-module extraction described above
Phase 7   Relics
```
