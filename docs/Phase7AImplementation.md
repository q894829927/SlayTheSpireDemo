# Phase 7A — Relic Runtime Implementation

Date: **2026-08-22**

Status: **7A-1 / 7A-2 SOURCE IMPLEMENTED; 7A-3 PENDING; UE5.8 VALIDATION NOT RUN**

Branch: `phase7-relic-gameplay`

This document records implementation progress against `docs/Phase7EarlyDevelopmentBoundary.md`.

## Implemented in the first Phase 7A slice

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
└── GetRelics()
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

## Focused Automation authored

Prefix:

```text
SlayTheSpireDemo.Phase7A
```

Three tests are currently authored:

```text
Runtime.MembershipAndIdentity
Runtime.InvalidAndReset
Runtime.DefinitionIsolation
```

Coverage includes:

```text
valid creation
logical vs exact runtime identity
duplicate no-op semantics
invalid null/None definition input
stable insertion enumeration
non-zero/monotonic runtime sequence expectations
explicit Battle context
definition/runtime object separation
Container reset/reinitialize behavior
```

A dedicated workflow is available at:

```text
.github/workflows/ue-phase7a-tests.yml
```

The workflow builds `SlayTheSpireDemoEditor` on the UE5.8 self-hosted runner and requires exactly 3 focused Phase7A tests.

## Still pending for Phase 7A

### 7A-3 — Battle ownership + initialization + restart lifecycle

Not yet implemented in the first slice.

Required next work:

```text
ABattleManager owns the authoritative PlayerRelicContainer
small explicit battle setup/injection path for starting Relic definitions
StartBattle/new battle runtime clears previous Relic membership
new battle runtime cannot observe stale previous-battle Relic instances
focused restart-lifecycle Automation
```

Do not begin Phase 7B Trigger integration before this ownership/lifecycle slice is closed.

### 7A-4 — Validation / affected regression

Source tests and CI gate are authored, but no UE5.8 run is claimed yet.

Required before closing Phase 7A:

```text
UE5.8 Editor build PASS
Phase7A focused Automation PASS
affected Phase5/Phase6 regression PASS
static source review PASS
```

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
