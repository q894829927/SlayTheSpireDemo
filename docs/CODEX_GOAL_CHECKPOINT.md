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
7C Sundial + GainEnergyAction: IMPLEMENTED / VALIDATION PENDING
7D Relic Read/Frozen/Native UI: NOT STARTED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

Validation authorities already sealed:

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
Sundial counter mutation occurs through USundialAdvanceAction
Sundial reward occurs through UGainEnergyAction
A3 does not predict Relic reactions
```

## Accepted predecessor evidence

7A user-reported UE 5.8 gate:

```text
Development Editor Build                         PASS
SlayTheSpireDemo.Phase7.RelicRuntime            5/5 PASS
```

7B user-reported UE 5.8 gate:

```text
SlayTheSpireDemo.Phase7.TriggerSources          3/3 PASS
SlayTheSpireDemo.Phase6A.Trigger                PASS
```

Do not rerun those sealed gates merely because 7C adds concrete Relic content.

## 7C implementation now on main

Reusable Energy primitive:

```text
BattleEnergyMutation::TryGain
- Amount > 0
- no MaxEnergy clamp
- invalid Battle / invalid amount / int32 overflow fail soft
- exact Before / After / Delta result

UGainEnergyAction
- owns intended positive Amount
- commits only through TryGain
- emits existing EnergyChanged Presentation payload when a writer is available
- finishes on success or fail-soft rejection
```

Sundial runtime/content:

```text
URelicInstance
- Counter runtime state starts at 0
- public read accessor
- mutation boundary restricted to USundialAdvanceAction

USundialTrigger
- reacts only to FDeckShuffledEvent
- requires the current authoritative Battle DeckRuntime
- requires a valid Relic source and Battle
- is read-only
- freezes ShufflesRequired / EnergyGain into USundialAdvanceAction

USundialAdvanceAction
- validates exact current RelicInstance membership and frozen config
- 0 -> 1
- 1 -> 2
- threshold: queues dependent UGainEnergyAction(+2), then commits Counter -> 0
- propagates PresentationRecordWriter to the dependent Energy Action
- never pumps the queue
```

`ABattleManager::IsAuthoritativeDeckRuntime()` is the narrow Gameplay identity query used by Sundial; the Trigger does not recover or compare DeckRuntime through UObject Outer chains.

No card identity, DrawAction identity, RetryDraw identity or Pommel Strike special case exists.

## 7C focused Automation now on main

Energy prefix:

```text
SlayTheSpireDemo.Phase7.EnergyGain
- MutationContracts
- ActionAndPresentation
```

Covers:

```text
+2 succeeds
may exceed MaxEnergy
0 rejected
negative rejected
overflow rejected
invalid Battle fails soft
GainEnergyAction commits through queue
EnergyChanged Before / After / Delta exact
```

Sundial prefix:

```text
SlayTheSpireDemo.Phase7.Sundial
- SequenceAndDeckIdentity
- TriggerReadOnlyAndFrozenConfig
```

Covers:

```text
wrong DeckRuntime does not advance
0 -> 1
1 -> 2
2 -> 0 +2 Energy
0 -> 1
Trigger/BuildReactions do not mutate Counter or Energy
3 / +2 are frozen into the queued Action at BuildReactions time
```

The existing Phase 6 producer contract already proves setup shuffle emits no gameplay `FDeckShuffledEvent`; 7C does not duplicate that sealed producer test.

## Production Sundial asset status

The binary UE DataAsset cannot be authored safely through the text GitHub contents path. After the 7C C++ Build succeeds, create/configure locally in UE 5.8:

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2
```

Then add `DA_Relic_Sundial` to the production/test `BP_BattleManager -> DebugStartingRelics` array. Icon/HUD display belongs to 7D and is not required for the 7C Gameplay gate.

## Required 7C validation gate

Run only:

```text
1. Development Editor Build once.
2. SlayTheSpireDemo.Phase7.EnergyGain once; expected 2/2.
3. SlayTheSpireDemo.Phase7.Sundial once; expected 2/2.
4. No manual PIE gate yet; visible Relic acceptance belongs to 7D.
5. Record evidence and STOP.
```

Do not rerun Phase6R, A2D5, Shipping, Legacy parity or unrelated UI suites without a concrete failure.

## Next exact action

USER ACTION REQUIRED:

Build current `main`. If it compiles, run the two 7C focused prefixes above. If both pass, author/configure `DA_Relic_Sundial` locally and report the result.

Do not begin 7D before 7C is accepted and the production Sundial definition is present.
