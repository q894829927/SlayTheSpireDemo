# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-03**

## Goal

Implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A contracts or modeling Relics as Statuses.

## Current status

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED
Phase 6UI-A3: COMPLETE / VALIDATED / SEALED

Phase 7 Relics: IN PROGRESS
Phase 7 design: SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: NEXT / NOT STARTED
7D Relic Read/Frozen/Native UI: NOT STARTED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

Validation authorities:

```text
docs/Phase7AValidation.md
docs/Phase7BValidation.md
```

## Locked Phase 7 boundaries

```text
Relic != Status
RelicData != RelicInstance
Relics use the battle-wide ABattleManager RuntimeSequence allocator
Status + Relic trigger order = Priority → RuntimeSequence → LocalTriggerIndex
BattleEventDispatcher remains snapshot-based; no persistent Trigger Registry
Trigger remains read-only eligibility + Action construction
Sundial counter mutation will occur through an Action
Sundial reward will use GainEnergyAction
A3 does not predict Relic reactions
```

The design-review clarifications remain locked:

```text
- no first-version RelicTriggered / RelicCounterChanged Presentation Record
- Relic counter catches up at Envelope FinalSnapshot reconciliation
- Starting Relics are created explicitly during StartBattle, never lazily from a getter
- future HUD Relic DTO uses bShowCounter / Counter / CounterMax
- SundialAdvanceAction freezes ShufflesRequired / EnergyGain at reaction-build time
- frozen HUD DTOs may hold immutable presentation asset references but not mutable RelicInstance truth
- GainEnergyAction receives its own primitive tests in 7C
```

## 7A accepted validation

Validated implementation HEAD before evidence-only documentation updates:

```text
86de988bef8e85c17d6197394f72cf756627e693
```

User-reported UE 5.8 evidence on 2026-09-02:

```text
Development Editor Build                         PASS
SlayTheSpireDemo.Phase7.RelicRuntime            5/5 PASS
Manual PIE                                      NOT REQUIRED FOR 7A
```

7A is sealed. Do not rerun it merely because later slices change unrelated boundaries.

## 7B implementation and accepted validation

7B generalized only the Trigger-source boundary:

```text
FTriggerRuntimeSource
- SourceKind: Status / Relic
- RuntimeObject
- SourceId
- RuntimeSequence
- CombatantOwner (null for battle-owned Relic)

FTriggerContext
- preserves historical Status constructors/accessor behavior
- keeps GetRuntimeSource() as Status compatibility accessor
- adds GetRuntimeSourceObject()
- adds GetRelicSource()
- adds GetSourceKind()
- adds GetSourceId()
- adds GetRuntimeSequence()
- Relic contexts use the neutral descriptor rather than a pointer overload

URelicData
- now authors Instanced Triggers[]

BattleEventDispatcher
- still snapshots candidates at dispatch time
- collects Status candidates from supplied Combatants
- collects Relic candidates from the bound BattleContext RelicContainer
- deduplicates runtime source enumeration with TSet<UObject*>
- combines both source kinds into one candidate array
- sorts only by Priority → RuntimeSequence → LocalTriggerIndex
- does not use SourceKind as an ordering key
- preserves atomic final reaction insertion

FTriggerEligibilityRecord
- SourceKind + SourceId are the neutral fields
- StatusId remains as a Phase 6 compatibility field
- Relic records leave StatusId=None
```

User-reported UE 5.8 validation on 2026-09-03:

```text
SlayTheSpireDemo.Phase7.TriggerSources     3/3 PASS
SlayTheSpireDemo.Phase6A.Trigger          PASS
Manual PIE                                NOT REQUIRED FOR 7B
```

The prescribed Development Editor build step produced a runnable current-main test binary; no build/runtime failure was reported.

Formal evidence:

```text
docs/Phase7BValidation.md
```

7B is therefore **COMPLETE / VALIDATED / SEALED**. Do not run the full Phase6R aggregate, A2D5, Shipping, Legacy parity or unrelated UI suites merely because this source-neutral boundary is sealed.

## Next exact action

The next phase boundary is:

```text
Phase 7C — Sundial + GainEnergyAction
```

7C may now implement only:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
URelicInstance Sundial counter state required by the concrete vertical slice
USundialTrigger
USundialAdvanceAction
focused primitive Energy tests
focused Sundial gameplay tests
```

The locked behavior remains:

```text
initial setup shuffle: no FDeckShuffledEvent -> no Sundial progress
1st gameplay shuffle: 0 -> 1
2nd gameplay shuffle: 1 -> 2
3rd gameplay shuffle: 2 -> 0 + enqueue GainEnergyAction(+2)
4th gameplay shuffle: 0 -> 1
```

`USundialTrigger` is read-only and freezes `ShufflesRequired / EnergyGain` into the reaction Action at BuildReactions time. `USundialAdvanceAction` owns counter mutation and enqueues the reusable `UGainEnergyAction`; neither Trigger nor UI may mutate Gameplay truth directly.

Do not begin 7D Relic Read/Frozen/Native UI, Abacus, Phase 8, Relic modifiers, run persistence or advanced Relic Presentation in the same change.
