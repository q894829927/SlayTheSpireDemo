# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-02**

## Goal

Implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A contracts or modeling Relics as Statuses.

## Current status

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED
Phase 6UI-A3: COMPLETE / VALIDATED / SEALED

Phase 7 Relics: IN PROGRESS
Phase 7 design: SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: IMPLEMENTED / VALIDATION PENDING
7C Sundial + GainEnergyAction: NOT STARTED
7D Relic Read/Frozen/Native UI: NOT STARTED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

7A validation authority:

```text
docs/Phase7AValidation.md
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

7A is sealed. Do not rerun it merely because 7B changes the Dispatcher boundary unless a concrete failure implicates Relic runtime ownership/setup.

## 7B implementation now on main

Implementation HEAD before this checkpoint update:

```text
a1ec9e091d21a6f37c751bb0dca3f1a17b0a7e38
```

7B changes only the Trigger-source boundary:

```text
FTriggerRuntimeSource
- SourceKind: Status / Relic
- RuntimeObject
- SourceId
- RuntimeSequence
- CombatantOwner (null for battle-owned Relic)

FTriggerContext
- keeps the historical Status constructors
- keeps GetRuntimeSource() as the Status compatibility accessor
- adds GetRuntimeSourceObject()
- adds GetRelicSource()
- adds GetSourceKind()
- adds GetSourceId()
- adds GetRuntimeSequence()
- Relic contexts are created through the neutral descriptor, avoiding pointer-overload ambiguity

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
- adds SourceKind + SourceId
- preserves StatusId for historical Phase 6 tests
- Relic records leave StatusId=None
```

Explicit non-scope remains:

```text
no Sundial Trigger
no Relic Counter
no GainEnergyAction
no Relic Presentation
no Relic HUD
no Modifier-source generalization
no Trigger Registry
```

## 7B focused tests

New prefix:

```text
SlayTheSpireDemo.Phase7.TriggerSources
```

Current tests:

```text
ContextCompatibility
RelicReactionParticipation
CombinedOrderingAndTrace
```

They prove:

```text
- historical Status GetRuntimeSource()/Owner behavior remains available
- neutral Status/Relic context accessors report the correct source identity
- a battle-owned Relic Trigger participates in the real Dispatcher and builds an Action
- Relic eligibility trace uses SourceKind/SourceId and does not fake StatusId
- Status + Relic candidates execute in one Priority → RuntimeSequence → LocalTriggerIndex domain
- Starting Relics remain earlier than subsequently-created runtime Statuses when priority ties
```

## Required 7B validation gate

No current-main Build or Automation result is claimed for the 7B implementation yet.

Run only:

```text
1. Development Editor Build once.
2. SlayTheSpireDemo.Phase7.TriggerSources once; expected 3/3.
3. SlayTheSpireDemo.Phase6A.Trigger once as the smallest existing Dispatcher/ordering regression prefix directly invalidated by 7B.
4. No manual PIE gate for 7B.
5. Record evidence and STOP.
```

Do not run the full Phase6R aggregate, A2D5, Shipping, Legacy parity or unrelated UI suites without a concrete failure.

## Next exact action

USER ACTION REQUIRED:

Run the 7B Build + focused Automation gate above. If both the new 7B prefix and the existing Phase6A Trigger regression prefix pass, record:

```text
Phase 7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
Phase 7C Sundial + GainEnergyAction: NEXT / NOT STARTED
```

Do not begin 7C code before 7B acceptance is recorded.
