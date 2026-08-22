# Phase 7A — Relic Runtime Static Source Review

Date: **2026-08-22**

Status: **VALIDATED / COMPLETE / READY FOR PHASE 7B**

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

## 2. Runtime model review

`URelicData` owns immutable authored definition data only:

```text
RelicId
DisplayName
Description
```

`URelicInstance` owns concrete battle runtime identity:

```text
Definition
Battle
RuntimeSequence
```

The base instance has no generic Counter/state bag, Trigger list or Modifier list.

`URelicContainer::AddRelic()` exposes explicit outcomes:

```text
Invalid
Duplicate
Added
```

One active member per non-empty RelicId is enforced before runtime creation. Ordered `TArray` storage preserves deterministic enumeration.

## 3. Runtime identity and lifecycle review

Relics reuse:

```text
ABattleManager::AllocateRuntimeSequence()
```

Within one battle allocator session, tests require non-zero, unique and monotonic runtime identities without assuming contiguous `+1` allocation.

Across BattleIds, RuntimeSequence may restart. Therefore cross-battle identity must never be interpreted as RuntimeSequence alone.

The authoritative player Relic runtime is:

```text
ABattleManager
└── PlayerRelicContainer
```

The Container UObject may persist across `StartBattle()` calls, but membership is session-scoped by BattleId and is rebuilt through `GetPlayerRelicContainer()` from `DebugStartingRelics`.

Durable rule for Phase 7B+:

```text
Do not cache Relic membership across BattleId changes.
Resolve current membership through the current BattleManager/Container path.
Queued exact-instance work must validate current exact membership before mutation.
```

## 4. Static/UHT review

- Generated-header placement follows Unreal header rules.
- Runtime UObject ownership is explicit.
- DataAsset/runtime state separation is preserved.
- No new Runtime module dependency was introduced.
- The existing Editor test module include setup supports `Relics/...` headers.
- No high-confidence C++/UHT blocker was identified statically.

The subsequent UE5.8 Editor build passed, closing the remaining compile uncertainty.

## 5. Focused and regression validation

Validated Phase 7A focused tests:

```text
Runtime.MembershipAndIdentity
Runtime.InvalidAndReset
Runtime.DefinitionIsolation
Runtime.BattleRestartLifecycle
```

Validation result:

```text
UE5.8 Editor build                     PASS
Phase7A focused Automation             4/4 PASS
Affected Phase5/6/UI-A2 regression    84/84 PASS
Static source review                   PASS
```

Formal record:

```text
docs/Phase7AValidation.md
```

## 6. Closure state

```text
7A-1 Definition + Runtime Identity       PASS
7A-2 Container Membership               PASS
7A-3 Battle Ownership / Restart          PASS
7A-4 Focused gate                       4/4 PASS
7A-4 Affected regression               84/84 PASS
Static source review                     PASS
UE5.8 Editor build                       PASS
```

Therefore:

```text
Phase 7A COMPLETE
Phase 7B READY TO DESIGN / IMPLEMENT
```
