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
7C Sundial + GainEnergyAction: IMPLEMENTED / BULK-DRAW REFACTOR / VALIDATION PENDING
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

Do not rerun those sealed gates because of the 7C draw refactor.

## 7C Relic/Energy implementation on main

Reusable Energy primitive:

```text
BattleEnergyMutation::TryGain
UGainEnergyAction
```

Sundial runtime/content:

```text
URelicInstance::Counter
USundialTrigger
USundialAdvanceAction
```

Sundial still reacts only to the authoritative battle `FDeckShuffledEvent`; no card identity, DrawAction identity, RetryDraw identity or Pommel Strike special case exists.

## 7C draw-semantics refactor

The first zero-card correction modeled `Draw N` as N independent draw attempts. Before sealing 7C, that temporary model was replaced with a source-game-style bulk draw boundary.

Current production structure:

```text
UDrawCardEffect DrawCount=N
→ UDrawCardsAction(N)
   - owns RemainingDraws
   - evaluates Hand capacity + DrawPile + DiscardPile at Execute time
   - plans deterministic continuation batches
   ↓
   UDrawCardAction x available-now
   → UShuffleDeckAction when the bulk request still owes draws
   → UDrawCardsAction(Remaining)

UDrawCardAction
= one atomic DrawPile -> Hand commit only
= no shuffle planning
= no retry recursion
```

`ABattleManager::BuildDrawActionBatch()` now also creates one `UDrawCardsAction(DrawCount)` so opening-hand and turn-start draw use the same semantics as card effects. The debug single-draw path uses `UDrawCardsAction(1)` as well.

## Correct zero-card shuffle semantics

A **fresh** bulk request against a truly exhausted deck does not shuffle:

```text
BulkDraw(1)
Draw=0 / Discard=0
→ end
→ no DeckShuffled
```

A zero-card shuffle can still happen when it was already planned by an earlier bulk step:

```text
BulkDraw(2)
initial Draw=0 / Discard=1
→ Shuffle #1
→ BulkDraw(2) sees Draw=1 / Discard=0
→ plans DrawCard(1) + Shuffle #2 + BulkDraw(1)
→ DrawCard consumes the only card
→ planned Shuffle #2 executes at Draw=0 / Discard=0
   MovedCardCount=0
   DeckShuffled #2
→ final BulkDraw(1) sees a truly exhausted deck and ends
```

This is the generic two-Pommel-Strike+/Sundial behavior. `UShuffleDeckAction` retains zero-card commit ability when it is legitimately scheduled at an empty-DrawPile boundary.

Durable producer history is amended in:

```text
docs/Phase6CImplementation.md
```

## Focused Automation on current main

Energy prefix remains:

```text
SlayTheSpireDemo.Phase7.EnergyGain
- MutationContracts
- ActionAndPresentation
```

Sundial prefix:

```text
SlayTheSpireDemo.Phase7.Sundial
- SequenceAndDeckIdentity
- DrawTwoCountsZeroCardShuffle
- TriggerReadOnlyAndFrozenConfig
```

`DrawTwoCountsZeroCardShuffle` now uses **one `UDrawCardsAction(2)`**, not two independent single-draw actions.

Phase6C prefix now contains 6 tests, adding:

```text
SlayTheSpireDemo.Phase6C.Draw.EmptyBulkDoesNotShuffle
```

This proves a fresh `Draw=0 / Discard=0` bulk request does not manufacture a shuffle event.

## Production Sundial asset

Expected local UE asset:

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2
```

Icon/HUD display remains 7D.

## Required current-head validation gate

The bulk draw refactor changes shared Draw/Shuffle producer code, so validate only the directly invalidated contract:

```text
1. Regenerate project files once because DrawCardsAction.h/.cpp are new.
2. Development Editor Build once.
3. SlayTheSpireDemo.Phase6C once; expected 6/6.
4. SlayTheSpireDemo.Phase7.Sundial once; expected 3/3.
5. SlayTheSpireDemo.Phase7.EnergyGain does not need rerun unless it had not already passed; Energy code is unchanged by the bulk refactor.
6. One focused PIE check with configured Sundial + two upgraded draw-two Pommel Strikes:
   - after other cards are exhausted, one Draw 2 with one recyclable card produces the real + planned zero-card shuffle pair;
   - a fresh draw against Draw=0 / Discard=0 does not create extra shuffle counts;
   - Sundial grants +2 every third committed shuffle;
   - the two-card loop can remain infinite as expected.
7. Record evidence and STOP.
```

Do not rerun Phase6R, A2D5, Shipping, Legacy parity or unrelated UI suites without a concrete failure.

## Next exact action

USER ACTION REQUIRED:

Regenerate project files, build current `main`, run the Phase6C and Phase7.Sundial focused prefixes, then repeat the two-upgraded-Pommel + Sundial PIE check.

Do not begin 7D before corrected 7C behavior is accepted.
