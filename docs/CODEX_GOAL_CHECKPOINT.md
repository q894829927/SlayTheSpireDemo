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

## 7C gameplay-fidelity correction — zero-card shuffle

Manual use of two upgraded Pommel-Strike-style draw-two cards with Sundial exposed one mismatch with Slay the Spire 1: the project previously treated `DrawPile=0, DiscardPile=0` as no shuffle, which prevents the standard Sundial infinite.

The corrected generic draw contract is now:

```text
one authored draw attempt
↓
DrawPile non-empty
→ draw normally

DrawPile empty
→ exactly one ShuffleDeckAction
→ DiscardPile may contain cards OR be empty
→ ShuffleDiscardIntoDrawPileCommit succeeds when DrawPile is empty
→ zero-card shuffle uses MovedCardCount=0
→ DeckShuffled Record/Event still emits
→ shuffle reactions run
→ RetryDraw exactly once
→ if still empty, stop; do not shuffle recursively again
```

This produces the intended draw-two behavior:

```text
Draw=0, Discard=1
Draw #1 → shuffle one card → DeckShuffled #1 → draw it
Draw #2 → Draw=0, Discard=0 → zero-card shuffle → DeckShuffled #2 → no card drawn
```

Initial battle setup shuffle remains excluded. A non-empty DrawPile still rejects an explicit gameplay shuffle. No concrete card/relic special case was added.

Durable Phase 6C history has been amended in:

```text
docs/Phase6CImplementation.md
```

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
- DrawTwoCountsZeroCardShuffle
- TriggerReadOnlyAndFrozenConfig
```

Covers:

```text
wrong DeckRuntime does not advance
0 -> 1
1 -> 2
2 -> 0 +2 Energy
0 -> 1
one-card discard + draw-two produces two shuffle counts
second draw may commit a zero-card shuffle
RetryDraw after zero-card shuffle terminates without recursion/fault
Trigger/BuildReactions do not mutate Counter or Energy
3 / +2 are frozen into the queued Action at BuildReactions time
```

The existing Phase 6 producer contract still proves setup shuffle emits no gameplay `FDeckShuffledEvent`; 7C does not change setup initialization.

## Production Sundial asset status

The intended production definition is:

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2
```

The user has exercised Sundial in gameplay; that manual run exposed the zero-card shuffle mismatch above. Icon/HUD display still belongs to 7D.

## Required 7C validation gate after zero-card correction

Because Draw/Shuffle producer semantics changed after the earlier Sundial run, the current-head gate is now:

```text
1. Development Editor Build once.
2. SlayTheSpireDemo.Phase7.Sundial once; expected 3/3.
3. SlayTheSpireDemo.Phase6C once; expected historical 5/5 regression prefix to remain green.
4. SlayTheSpireDemo.Phase7.EnergyGain does NOT need rerun if it already passed before this correction; this change does not touch Energy code/tests.
5. One focused manual PIE check with the configured Sundial + two upgraded draw-two Pommel Strikes:
   - after exhausting other cards, each draw-two cycle counts the real + zero-card shuffles correctly;
   - Sundial grants +2 every third shuffle;
   - the two-card loop can remain Energy-neutral/infinite as expected.
6. Record evidence and STOP.
```

Do not rerun Phase6R, A2D5, Shipping, Legacy parity or unrelated UI suites without a concrete failure.

## Next exact action

USER ACTION REQUIRED:

Build current `main`, run `SlayTheSpireDemo.Phase7.Sundial` and `SlayTheSpireDemo.Phase6C`, then repeat the two-upgraded-Pommel + Sundial PIE check that exposed this issue.

Do not begin 7D before this corrected 7C behavior is accepted.
