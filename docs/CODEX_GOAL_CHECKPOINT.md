# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-02**

## Goal

Design and then implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A contracts or modeling Relics as Statuses.

## Current status

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED
Phase 6UI-A3: COMPLETE / VALIDATED / SEALED

Phase 7 Relics: DESIGN AUTHORIZED / IMPLEMENTATION NOT STARTED
7A Relic Runtime: NEXT IMPLEMENTATION SLICE, NOT YET AUTHORIZED TO CODE
7B Status + Relic Trigger Sources: NOT STARTED
7C Sundial + GainEnergyAction: NOT STARTED
7D Relic Read/Frozen/Native UI: NOT STARTED
```

Final A3 seal authority:

```text
docs/Phase6UIA3Seal.md
```

Phase 7 active design authority:

```text
docs/Phase7RelicsImplementation.md
```

`docs/Phase6UIA3Implementation.md` remains the detailed historical implementation plan and durable A3 contract document; its old progress header is superseded by `docs/Phase6UIA3Seal.md`.

## Phase 6UI-A final accepted evidence

User-reported UE 5.8 evidence on 2026-09-02:

```text
SlayTheSpireDemo.UIA3.ViewModelPreviewLifecycle: 3/3 PASS
SlayTheSpireDemo.UIA3.NativePreviewIntegration: 3/3 PASS
SlayTheSpireDemo.UIA3.RichCardTextBaseline: 2/2 PASS
Production L_BattleTest PIE: CardPlayed animation restored
Production PIE: Strength-modified current card-face Damage colors correctly
```

No playable Dexterity-granting card currently exists. Dexterity/Frailty Block RichText is accepted through the passing focused `BlockTracksDexterityAndFrailty` Automation and is not a Phase 7 blocker.

Do not rerun these gates merely because Phase 7 begins.

## Phase 7 design baseline

Design document commit:

```text
27756788f0945a2d2550346e5e36a3cb618277a7
docs(phase7): design relic runtime and sundial vertical slice
```

Project roadmap seal/update commit immediately before the Phase 7 design:

```text
16a4cd954286e2c0b32ef0bfb05b67925d9aac6f
docs: seal phase 6ui-a and authorize phase 7 design
```

A3 final seal document commit:

```text
a42fe2c1e4fd8728f264bb67a03c09b378338cda
docs(ui-a3): seal playable immediate preview phase
```

## Locked Phase 7 architecture

```text
RelicData != RelicInstance
Relic != Status
Relic mutable progress belongs to RelicInstance
RelicContainer is battle-scoped until a real run layer exists
Relics use ABattleManager::AllocateRuntimeSequence()
Status + Relic trigger order remains Priority → RuntimeSequence → LocalTriggerIndex
BattleEventDispatcher remains snapshot-based
No persistent Trigger Registry
UBattleTrigger remains read-only
Sundial counter mutation occurs through an Action
Sundial reward uses a reusable GainEnergyAction
A3 does not predict Relic reactions
No Relic Modifier framework is introduced until a concrete modifier Relic requires it
```

## Sundial target behavior

```text
initial setup shuffle: no FDeckShuffledEvent -> no progress
1st gameplay shuffle: counter 0 -> 1, no Energy
2nd gameplay shuffle: counter 1 -> 2, no Energy
3rd gameplay shuffle: counter 2 -> 0, enqueue dependent GainEnergyAction(+2)
4th gameplay shuffle: counter 0 -> 1
```

The Trigger only builds the reaction Action. It does not mutate the Relic counter or Energy.

## Planned slices

### 7A — Relic Runtime

```text
URelicData
URelicInstance
URelicContainer
BattleManager ownership/setup
battle-wide RuntimeSequence
focused runtime tests
```

No Dispatcher, Sundial or UI changes in 7A.

### 7B — Status + Relic Trigger Sources

Generalize only the current Status-shaped Trigger runtime-source boundary. Preserve old Status trigger behavior and ordering. Do not add a registry.

### 7C — Sundial + GainEnergyAction

Add positive Energy mutation Action plus Sundial Trigger/Advance Action. Initial setup shuffle remains excluded by the existing event contract.

### 7D — Relic Read/Frozen/Native UI

Expose Relic state through read/frozen DTOs and the Native HUD. No dedicated RelicTriggered Presentation Record is required for the first slice; existing DeckShuffled/EnergyChanged playback plus FinalSnapshot catch-up is sufficient.

## Next exact action

Wait for explicit user authorization to implement **Phase 7A — Relic Runtime**.

When authorized, inspect only the exact current battle setup and RuntimeSequence allocation path needed to place `URelicContainer` deterministically, then implement 7A as one bounded slice.

Do not begin 7B, 7C, Abacus, Phase 8, Relic modifiers, run persistence or advanced Relic Presentation in the same change.
