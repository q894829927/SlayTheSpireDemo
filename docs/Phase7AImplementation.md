# Phase 7A — Relic Runtime Implementation

Date: **2026-08-22**

Status: **SOURCE IMPLEMENTATION + STATIC REVIEW COMPLETE / UE5.8 VALIDATION NOT RUN**

Branch: `phase7-relic-gameplay`

This document records implementation progress against `docs/Phase7EarlyDevelopmentBoundary.md`.

## Implemented

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

Current membership rules:

```text
RelicId must be non-empty
one active member per RelicId in one Container
duplicate add returns existing instance and does not add membership
ordered TArray storage preserves deterministic insertion enumeration
runtime object creation uses the existing battle-scoped sequence allocator
```

### 7A-3 — Battle ownership + initialization + restart lifecycle

Implemented without changing the existing `StartBattle()` body.

`ABattleManager` owns the authoritative player Relic runtime through:

```text
ABattleManager
└── PlayerRelicContainer (private/transient)
```

Formal access is:

```text
ABattleManager::GetPlayerRelicContainer()
```

The accessor lazily creates the Container as a Battle-owned UObject and synchronizes it to the current `BattleId`.

Battle setup uses the temporary/demo injection surface:

```text
DebugStartingRelics[]
```

On first access in a Battle session:

```text
GetPlayerRelicContainer()
↓
create Container if needed
↓
if BattleId changed
    Initialize(Battle)
    clear previous membership
    replay DebugStartingRelics in authored array order
↓
return current-battle Container
```

This avoids a large parallel edit to the actively changing `BattleManager.cpp` while still making BattleId the runtime-session boundary.

On battle restart:

```text
StartBattle()
↓
BattleId advances
↓
next formal RelicContainer access detects the new BattleId
↓
old membership is cleared
↓
new runtime instances are created from DebugStartingRelics
```

RuntimeSequence is battle-scoped and may restart between BattleIds. Exact historical identity therefore must never be interpreted as RuntimeSequence alone across battles.

## Focused Automation authored

Prefix:

```text
SlayTheSpireDemo.Phase7A
```

Four tests are authored:

```text
Runtime.MembershipAndIdentity
Runtime.InvalidAndReset
Runtime.DefinitionIsolation
Runtime.BattleRestartLifecycle
```

Coverage includes:

```text
valid creation
BattleManager-owned Container
logical vs exact runtime identity
duplicate no-op semantics
invalid null/None definition input
stable insertion enumeration
non-zero/monotonic runtime sequence expectations within one allocator session
explicit Battle context
definition/runtime object separation
Container reset/reinitialize behavior
DebugStartingRelics setup injection
BattleId restart detection
old exact runtime instance isolation after restart
current-battle membership rebuild
```

## Validation gate authored

A dedicated workflow is available at:

```text
.github/workflows/ue-phase7a-tests.yml
```

It is configured to run:

```text
UE5.8 Editor build
Phase7A focused Automation              4 tests
existing affected Phase5/6/UI-A2 gate 84 tests
```

Affected regression prefixes:

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

Static review is recorded in:

```text
docs/Phase7ASourceReview.md
```

No high-confidence C++/UHT blocker was identified statically, but this is not a compiler result.

## Still pending for Phase 7A

Actual UE5.8 execution has not been run/recorded yet.

Required before closing Phase 7A:

```text
UE5.8 Editor build                     PASS
Phase7A focused Automation             4/4 PASS
Affected Phase5/6/UI-A2 regression    84/84 PASS
Static source review                   COMPLETE
```

Until those runtime/build results exist, the correct claim is:

```text
Phase 7A source implementation complete and ready for UE5.8 validation.
```

Do not begin Phase 7B Trigger integration until the validation gate is closed.

## Scope guard

The current source intentionally does not contain:

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
