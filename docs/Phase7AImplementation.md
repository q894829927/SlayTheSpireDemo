# Phase 7A — Relic Runtime Implementation

Date: **2026-08-22**

Status: **COMPLETE / VALIDATED / READY FOR PHASE 7B**

Branch: `phase7-relic-gameplay`

This document records the completed Phase 7A implementation against `docs/Phase7EarlyDevelopmentBoundary.md`.

## 1. Implemented runtime foundation

### 7A-1 — Relic Definition + Runtime Identity

Implemented:

```text
URelicData
├── RelicId
├── DisplayName
└── Description

URelicInstance
├── Definition
├── explicit Battle context
├── RuntimeSequence
├── GetRelicId()
├── GetRuntimeSequence()
├── GetBattle()
└── GetDebugLabel()
```

The base runtime instance intentionally contains no generic Counter/state bag and no Trigger/Modifier configuration.

RuntimeSequence allocation reuses `ABattleManager::AllocateRuntimeSequence()`.

### 7A-2 — RelicContainer Membership / Duplicate / Lookup

Implemented:

```text
URelicContainer
├── Initialize(Battle)
├── Reset()
├── AddRelic()
├── FindRelicById()
├── ContainsRelic()
├── ContainsRelicInstance()
├── GetRelics()
└── GetBattle()
```

Typed add semantics:

```text
ERelicAddOutcome
├── Invalid
├── Duplicate
└── Added
```

Membership rules:

```text
RelicId must be non-empty
one active member per RelicId in one Container
duplicate add returns the existing instance and does not add membership
ordered TArray storage preserves deterministic insertion enumeration
runtime object creation uses the existing battle-scoped sequence allocator
```

### 7A-3 — Battle ownership + initialization + restart lifecycle

`ABattleManager` owns the authoritative player Relic runtime:

```text
ABattleManager
└── PlayerRelicContainer (private/transient)
```

Formal access:

```text
ABattleManager::GetPlayerRelicContainer()
```

Temporary/demo setup input:

```text
DebugStartingRelics[]
```

Runtime session flow:

```text
GetPlayerRelicContainer()
↓
create Container if needed
↓
if cached BattleId != current BattleId
    Initialize(Battle)
    clear previous membership
    replay DebugStartingRelics in deterministic authored order
↓
return current-battle Container
```

Across `StartBattle()` calls, the Container UObject may remain owned by the BattleManager, but membership is rebuilt for the new BattleId. Old exact runtime instances are not current members after restart.

RuntimeSequence is battle-scoped and may restart across BattleIds, so cross-battle identity must never be interpreted as RuntimeSequence alone.

## 2. Focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase7A
```

Validated tests:

```text
Runtime.MembershipAndIdentity
Runtime.InvalidAndReset
Runtime.DefinitionIsolation
Runtime.BattleRestartLifecycle
```

Coverage includes:

```text
valid creation
BattleManager ownership
logical vs exact runtime identity
duplicate no-op semantics
invalid input isolation
stable ordered enumeration
same-session RuntimeSequence semantics
explicit Battle context
definition/runtime separation
Container reset/reinitialize
DebugStartingRelics setup injection
BattleId restart detection
old exact-instance isolation after restart
current-battle membership rebuild
```

## 3. Validation result

The Phase 7A GitHub Actions gate passed:

```text
UE5.8 Editor build                     PASS
Phase7A focused Automation             4/4 PASS
Affected Phase5/6/UI-A2 regression    84/84 PASS
Static source review                   PASS
```

Formal validation record:

```text
docs/Phase7AValidation.md
```

Static review:

```text
docs/Phase7ASourceReview.md
```

## 4. Scope guard

Phase 7A intentionally does not contain:

```text
Relic Trigger definitions
Sundial
DeckShuffled Relic handling
Relic counters
GainEnergyAction for Sundial
Relic Modifier contribution
Relic Presentation
Relic HUD / UMG
Phase 8 combo code
```

These remain later-phase work.

## 5. Closure

```text
7A-1 Definition + Runtime Identity       PASS
7A-2 Container Membership               PASS
7A-3 Battle Ownership / Restart          PASS
7A-4 Focused Automation                  4/4 PASS
7A-4 Affected regression                84/84 PASS
Static source review                     PASS
UE5.8 Editor build                       PASS
```

Therefore:

```text
Phase 7A COMPLETE
Phase 7B READY TO START
```
