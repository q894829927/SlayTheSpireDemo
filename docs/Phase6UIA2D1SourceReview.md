# Phase 6UI-A2D1 Static Source Review

Date: **2026-08-22**

Status: **VALIDATED / READY FOR A2D-2**.

Reviewed source base:

```text
4308d3bffe016e75f03a5f613a29ac0d426d0ec2
feat(ui-a2d): add exact status mutation results
```

Follow-up hardening in this review also removes the implicit `ActionQueue -> Outer -> ABattleManager` dependency from the turn-end Status trigger path before A2D-2 begins consuming BattleManager PresentationId resolution.

This review covers the A2D-1 Gameplay mutation slice and the explicit trigger Battle-context handoff.

## Scope reviewed

```text
Status/StatusMutationTypes.h
Status/StatusContainer.h/.cpp
Actions/ApplyStatusAction.cpp
Actions/ReduceStatusAction.h/.cpp
Actions/RemoveStatusAction.h/.cpp
Events/BattleTrigger.h/.cpp
Events/BattleEventDispatcher.h/.cpp
Events/TurnEndStatusDecayTrigger.cpp
Battle/BattleManager.h event-dispatch context handoff
SlayTheSpireDemoTests focused A2D-1 test source
SlayTheSpireDemoTests.Build.cs include/dependency surface
```

## Compile/UHT review

- `StatusMutationTypes.h` places its generated include after normal includes and exposes only the reflected `EStatusChangeReason`; the non-reflected `EStatusMutationOutcome` / `FStatusMutationResult` remain Gameplay C++ types.
- `FStatusMutationResult` stores raw `UStatusInstance*` / `UStatusData*` only for synchronous Action-layer consumption. It is not a reflected/asynchronous cache and therefore does not introduce Presentation ownership or GC-backed history.
- `StatusContainer.h` includes `StatusMutationTypes.h` directly, so all CommitResult API declarations have complete value types.
- `ReduceStatusAction.h` includes `StatusMutationTypes.h` before its generated header, making the reflected action header's `EStatusChangeReason` member visible to UHT/C++.
- `RemoveStatusAction.h` needs no mutation-type member in the header; its `.cpp` receives `FStatusMutationResult` through `StatusContainer.h`.
- `BattleEventDispatcher.h` owns a transient `TObjectPtr<ABattleManager>` and exposes explicit idempotent `BindBattleContext()` / `GetBattleContext()` APIs; no UObject Outer traversal is required to recover the authoritative battle.
- `BattleManager.h` now includes the dispatcher definition because its inline `TryBuildEventDispatchContext()` explicitly binds `this` before handing the dispatcher to runtime actions.
- `FTriggerContext` carries `ABattleManager*` independently from `ActionOuter`; the pre-A2D constructor overload remains for existing standalone tests/callers that do not require a Battle context.
- The Editor-only test module already depends on Core/CoreUObject/Engine/SlayTheSpireDemo and exposes the Runtime module root/private include paths needed by the focused test file.
- No new `.uasset`, `.umap` or module dependency is required for this hardening.

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

## Explicit Battle-context ownership hardening

The previous turn-end decay path constructed `UReduceStatusAction` by recovering BattleManager with:

```text
ActionOuter
→ ActionQueue
→ Queue.GetOuter()
→ Cast<ABattleManager>
```

That implicit object-ownership assumption is removed.

The formal path is now:

```text
ABattleManager::TryBuildEventDispatchContext
→ EventDispatcher.BindBattleContext(Battle)
↓
UBattleEventDispatcher::Dispatch
→ FTriggerContext(RuntimeSource, ActionOuter, Battle, Writer)
↓
UTurnEndStatusDecayTrigger::BuildReactions
→ Context.GetBattle()
↓
UReduceStatusAction::Initialize(Battle, Source, Target, ...)
```

`ActionOuter` remains responsible only for Action allocation / Queue batch ownership. It is no longer treated as the source of authoritative Battle identity.

`BindBattleContext()` is idempotent for the same Battle and rejects rebinding a live dispatcher to a different Battle. Standalone Dispatcher tests that never bind a Battle remain valid for pre-A2D no-history scenarios; A2D-2 Presentation producers can explicitly require a valid context when a writer is active.

The focused lifecycle test also verifies that `FTriggerContext` preserves an explicit Battle pointer when its `ActionOuter` is a Queue whose own Outer is deliberately **not** the BattleManager.

## Focused Automation

Exactly three A2D-1 focused top-level tests exist under the separate prefix:

```text
SlayTheSpireDemo.Phase6UIA2D1.Commit.StatusMutationLifecycle
SlayTheSpireDemo.Phase6UIA2D1.Action.StaleReduceDoesNotRetarget
SlayTheSpireDemo.Phase6UIA2D1.Action.ExactRemoveDoesNotRetarget
```

Coverage matrix:

```text
create                           covered
merge                            covered, including different incoming Definition with same StatusId
existing EffectiveDefinition     covered
MAX_int32 saturation no-op       covered
partial reduce                   covered
reduce-to-remove                 covered
stale exact reduce               covered
queued stale ReduceAction        covered
successful explicit RemoveAction covered
recreate same StatusId           covered
newer RuntimeSequence            covered without assuming contiguous +1
queued stale RemoveAction        covered
replacement untouched            covered
queue drains without fault       covered for stale/success action paths
explicit Dispatcher Battle bind  covered
TriggerContext Battle identity   covered with non-Battle ActionOuter
```

## Validation result

Validated source base:

```text
4b1b296a0c27d52d3b817207ca487ad32ef45e20
refactor(ui-a2d): pass explicit battle context to status triggers
```

User-reported UE5.8 validation result:

```text
A2D-1 mutation source             PASS
A2D-1 exact Reduce/Remove         PASS
URemoveStatusAction               PASS
TurnEndDecay reason context       PASS
explicit trigger Battle context   PASS
ActionQueue Outer dependency      REMOVED
Phase6UIA2D1 focused Automation   3/3 PASS
Phase6R affected regression       80/80 PASS
```

A2D-1 is therefore closed at the C++/Automation level and is ready for A2D-2.

See `docs/Phase6UIA2D1Validation.md` for the dedicated validation record.
