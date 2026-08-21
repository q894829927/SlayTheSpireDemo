# Phase 6UI-A2D1 Static Source Review

Date: **2026-08-22**

Status: **STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Reviewed source base:

```text
4308d3bffe016e75f03a5f613a29ac0d426d0ec2
feat(ui-a2d): add exact status mutation results
```

This review covers the A2D-1 Gameplay mutation slice only. It does not claim UnrealHeaderTool, MSVC, Unreal Editor, Automation, Blueprint or PIE execution.

## Scope reviewed

```text
Status/StatusMutationTypes.h
Status/StatusContainer.h/.cpp
Actions/ApplyStatusAction.cpp
Actions/ReduceStatusAction.h/.cpp
Actions/RemoveStatusAction.h/.cpp
Events/TurnEndStatusDecayTrigger.cpp
SlayTheSpireDemoTests focused A2D-1 test source
SlayTheSpireDemoTests.Build.cs include/dependency surface
```

## Compile/UHT review

- `StatusMutationTypes.h` places its generated include after normal includes and exposes only the reflected `EStatusChangeReason`; the non-reflected `EStatusMutationOutcome` / `FStatusMutationResult` remain Gameplay C++ types.
- `FStatusMutationResult` stores raw `UStatusInstance*` / `UStatusData*` only for synchronous Action-layer consumption. It is not a reflected/asynchronous cache and therefore does not introduce Presentation ownership or GC-backed history.
- `StatusContainer.h` includes `StatusMutationTypes.h` directly, so all CommitResult API declarations have complete value types.
- `ReduceStatusAction.h` includes `StatusMutationTypes.h` before its generated header, making the reflected action header's `EStatusChangeReason` member visible to UHT/C++.
- `RemoveStatusAction.h` needs no mutation-type member in the header; its `.cpp` receives `FStatusMutationResult` through `StatusContainer.h`.
- The Editor-only test module already depends on Core/CoreUObject/Engine/SlayTheSpireDemo and exposes the Runtime module root/private include paths needed by the new focused test file.
- No new `.uasset`, `.umap`, module dependency or Build.cs change is required for A2D-1.

No high-confidence C++ or UHT compile blocker was identified by static inspection.

## Mutation contract review

The source preserves the locked A2D-1 semantics:

```text
Apply create
→ Committed, 0 → Amount, bCreated=true

Apply merge
→ exact existing instance / existing RuntimeSequence / EffectiveDefinition

Apply at MAX_int32 with no numeric change
→ NoOp

Reduce exact instance with remaining amount
→ Committed, membership retained

Reduce exact instance to zero
→ Committed, bRemoved=true, membership removed

Reduce stale exact instance
→ NoOp, never retarget by StatusId

Remove exact instance
→ Committed, bRemoved=true

Remove stale exact instance
→ NoOp, never retarget replacement with same StatusId
```

Compatibility `ApplyStatus`, `ReduceStatus` and `RemoveStatusById` wrappers remain available for pre-A2D callers/tests; new queued removal uses `URemoveStatusAction` with exact instance identity.

## Focused Automation authored

Exactly three A2D-1 focused top-level tests are added under the separate prefix:

```text
SlayTheSpireDemo.Phase6UIA2D1.Commit.StatusMutationLifecycle
SlayTheSpireDemo.Phase6UIA2D1.Action.StaleReduceDoesNotRetarget
SlayTheSpireDemo.Phase6UIA2D1.Action.ExactRemoveDoesNotRetarget
```

Coverage matrix:

```text
create                         covered
merge                          covered, including different incoming Definition with same StatusId
existing EffectiveDefinition   covered
MAX_int32 saturation no-op     covered
partial reduce                 covered
reduce-to-remove               covered
stale exact reduce             covered
queued stale ReduceAction      covered
successful explicit RemoveAction covered
recreate same StatusId         covered
newer RuntimeSequence          covered without assuming contiguous +1
queued stale RemoveAction      covered
replacement untouched          covered
queue drains without fault     covered for stale/success action paths
```

## Non-blocking A2D-2 watch item

`UTurnEndStatusDecayTrigger` currently reconstructs the `ABattleManager*` supplied to the context-rich `UReduceStatusAction` from the ActionQueue outer. Under the current runtime BattleManager-owned queue this is valid, and A2D-1 mutation execution does not consume the Battle pointer. Before A2D-2 begins writing `StatusChanged` records, the producer path should explicitly re-check this ownership dependency because PresentationId resolution must not silently depend on an unavailable Battle pointer.

This is not an A2D-1 compile blocker and does not change exact-instance mutation behavior.

## Validation status

```text
A2D-1 mutation source             IMPLEMENTED
A2D-1 exact Reduce/Remove         IMPLEMENTED
URemoveStatusAction               IMPLEMENTED
TurnEndDecay reason context       IMPLEMENTED
Focused Automation source         AUTHORED: 3 tests
Static compile/UHT review         COMPLETE
UE5.8 Editor build                NOT RUN
Phase6UIA2D1 Automation           NOT RUN
Affected regression               NOT RUN
```

Do not mark A2D-1 runtime validation PASS until the UE5.8 build and focused Automation execute successfully.
