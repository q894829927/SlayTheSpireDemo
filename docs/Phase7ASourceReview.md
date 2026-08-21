# Phase 7A — Relic Runtime Static Source Review

Date: **2026-08-22**

Status: **STATIC REVIEW COMPLETE / UE5.8 BUILD AND AUTOMATION NOT RUN**

Branch: `phase7-relic-gameplay`

Reviewed source scope:

```text
Relics/RelicData.h/.cpp
Relics/RelicRuntimeTypes.h
Relics/RelicInstance.h/.cpp
Relics/RelicContainer.h/.cpp
Battle/BattleManager.h Phase7A additions
Battle/BattleManagerRelicRuntime.cpp
SlayTheSpireDemoTests/Private/Phase7ARelicRuntimeTests.cpp
.github/workflows/ue-phase7a-tests.yml
docs/Phase7AImplementation.md
```

## 1. Contract boundary review

The implementation stays inside the locked Phase 7A boundary.

Present:

```text
Relic definition/runtime/container model
logical RelicId
exact runtime UObject identity
battle-scoped RuntimeSequence
BattleManager-owned player RelicContainer
typed add outcome
ordered membership storage
battle restart/session rebuild
focused Automation
```

Absent by design:

```text
Relic Trigger types
Triggers[] on RelicData
Sundial
DeckShuffled Relic handling
Relic counters
GainEnergyAction for Sundial
Relic Modifier contribution
Relic Presentation
Relic HUD / UMG
Phase 8 combo code
```

No Phase 7B/7C implementation was introduced during Phase 7A.

## 2. UHT / C++ header review

- `RelicData.h`, `RelicInstance.h` and `RelicContainer.h` place their generated header after normal includes.
- `URelicData` mirrors the existing DataAsset pattern while intentionally exposing only `RelicId`, `DisplayName` and `Description` in 7A.
- `URelicInstance` is a runtime UObject with private initialization owned by `URelicContainer`; no mutable state is stored on the shared DataAsset.
- `FRelicAddResult` is a narrow synchronous C++ result and is intentionally non-reflected.
- `BattleManager.h` uses forward declarations for `URelicData` and `URelicContainer`; the repository already uses the same reflected `TObjectPtr`/`TArray<TObjectPtr<...>>` forward-declaration style for other UObject types.
- The Editor test module already exposes the Runtime module root as a private include path, so `Relics/...` includes do not require widening the Runtime module's public include surface.
- No new Runtime module dependency is required.

No high-confidence UHT or C++ compile blocker was identified by static inspection. This is not a substitute for the UE5.8 Editor build.

## 3. Definition/runtime separation

`URelicData` owns immutable authored definition data only:

```text
RelicId
DisplayName
Description
```

`URelicInstance` owns the concrete battle runtime identity:

```text
Definition
Battle
RuntimeSequence
```

The base instance intentionally has no generic `Counter`, `StateValue`, Trigger list or Modifier list.

The same `URelicData` can back multiple distinct runtime objects without sharing runtime mutation.

## 4. Membership / duplicate semantics

`URelicContainer::AddRelic()` has explicit outcomes:

```text
Invalid
Duplicate
Added
```

The current one-member-per-RelicId rule is enforced before runtime allocation.

Therefore a duplicate add:

```text
finds the existing member
returns Duplicate + existing Instance
does not allocate a replacement member
does not change ordered membership
```

Invalid input leaves membership unchanged.

The Container stores runtime members in an ordered `TArray`, so deterministic gameplay does not inherit unordered `TMap`/`TSet` iteration.

## 5. Runtime identity review

Relic instances reuse:

```text
ABattleManager::AllocateRuntimeSequence()
```

rather than introducing a Relic-only sequence domain.

Within one battle allocator session, tests require only:

```text
RuntimeSequence > 0
later created identity > earlier created identity
```

They do not require contiguous `+1` allocation.

Across different `BattleId` sessions, the existing BattleManager allocator may restart. Therefore cross-battle identity must never be interpreted as `RuntimeSequence` alone.

For future stale queued work, the exact runtime UObject instance must be validated against current Container membership. A same-`RelicId` replacement must not be treated as the old exact instance.

## 6. Battle ownership and restart lifecycle

The authoritative player Relic runtime is owned as:

```text
ABattleManager
└── PlayerRelicContainer
```

`PlayerRelicContainer` is private/transient and is exposed through:

```text
GetPlayerRelicContainer()
```

The Container object itself is retained across `StartBattle()` calls, but its membership is session-scoped by `BattleId`.

Formal synchronization behavior:

```text
GetPlayerRelicContainer()
↓
create Container if missing
↓
if the cached Container BattleId != current BattleId
    Initialize(this)
    clear old members
    rebuild DebugStartingRelics in deterministic array order
↓
return current-battle Container
```

This deliberately minimizes edits to the actively changing `BattleManager.cpp` while Phase 6UI-A2D is being developed in parallel.

### Durable rule for Phase 7B+

Code must not cache Relic membership across battle sessions and then continue using it after `StartBattle()`.

After a battle session changes, callers must resolve the current Relic runtime through the current BattleManager/Container path and validate exact instance membership before mutation.

The current lazy synchronization is acceptable for the isolated 7A runtime boundary because no Relic Trigger executes before formal RelicContainer access. If a later phase requires eager Relic initialization before any event dispatch, that phase should add the smallest explicit initialization call rather than bypassing this ownership contract.

## 7. Setup injection boundary

`DebugStartingRelics` is a temporary/demo battle setup input only.

It is not:

```text
Run inventory
SaveGame persistence
reward selection
map progression
permanent Relic collection architecture
```

The array order is the deterministic authored setup order used for runtime creation.

Invalid entries are ignored fail-soft while valid entries continue to initialize. Duplicate RelicIds resolve through the normal Container duplicate rule.

## 8. Focused Automation review

Exactly four focused tests are authored under:

```text
SlayTheSpireDemo.Phase7A
```

Tests:

```text
Runtime.MembershipAndIdentity
Runtime.InvalidAndReset
Runtime.DefinitionIsolation
Runtime.BattleRestartLifecycle
```

The suite covers:

```text
BattleManager ownership
valid runtime creation
logical and exact identity
non-zero/monotonic same-session sequence behavior
duplicate no-op semantics
invalid input isolation
stable insertion enumeration
explicit Battle context
definition/runtime separation
manual Container reset/reinitialize
BattleId advancement
restart membership rebuild
old exact-instance isolation after restart
```

The restart test intentionally does not require RuntimeSequence to increase across BattleIds.

## 9. CI / regression gate review

`.github/workflows/ue-phase7a-tests.yml` is manual owner-only self-hosted UE5.8 validation.

It is configured to:

```text
build SlayTheSpireDemoEditor
run Phase7A focused Automation, expected 4/4
run the existing affected Phase5/6/UI-A2 regression baseline, expected 84/84
```

The affected regression set mirrors the currently configured Phase6R source baseline:

```text
Phase5          13
Phase6A         23
Phase6B         12
Phase6C          5
Phase6UIA2A      8
Phase6UIA2B      8
Phase6UIA2C      8
Phase6UIA2D1     3
Phase6UIA2D2     4
------------------
Total           84
```

No PASS result is claimed until the self-hosted runner actually executes the workflow.

## 10. Current Phase 7A closure state

Static source state:

```text
7A-1 Definition + Runtime Identity       IMPLEMENTED
7A-2 Container Membership               IMPLEMENTED
7A-3 Battle Ownership / Restart          IMPLEMENTED
7A-4 Focused gate definition             IMPLEMENTED
7A-4 Affected regression definition      IMPLEMENTED
Static source review                     COMPLETE
UE5.8 Editor build                       NOT RUN
Phase7A focused Automation               NOT RUN
Affected 84-test regression              NOT RUN
```

Therefore the correct current claim is:

```text
Phase 7A source implementation complete and ready for UE5.8 validation.
```

Do not claim `Phase 7A COMPLETE` or begin Phase 7B until the required UE5.8 validation evidence is recorded.
