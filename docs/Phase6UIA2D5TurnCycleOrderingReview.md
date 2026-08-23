# Phase 6UI-A2D5-4 TurnCycleOrdering Review

Date: **2026-08-22**

Status: **VALIDATED / READY FOR A2D5-5 TERMINAL.VICTORY**.

Validated baseline after A2D5-4:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 3/3
Phase6R aggregate               PASS 97/97
Shipping exclusion              PASS
```

## Scenario

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.TurnCycleOrdering
```

File:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TurnCycleOrderingTest.cpp
```

The fixture forces every relevant visible turn-cycle fact to commit:

```text
Energy = 3
Hand = 3 concrete cards
DrawPile = 0
Discard = 0
Player Block = 7
Enemy Block = 5
Player TurnCycleDecay = 2 with TurnEndStatusDecayTrigger(-1)
Enemy committed attack = 3
PlayerTurnDrawCount = 2
```

All three opening-hand cards are discarded by EndTurn. This creates Discard=3 while DrawPile remains empty, so PlayerTurnStart must execute a real shuffle before drawing two cards.

The enemy attack is intentionally smaller than Player Block. Damage changes Player Block 7 -> 4 without HP loss, leaving a nonzero block for the later PlayerTurnStart clear 4 -> 0. This ensures the Player block-clear record is real rather than a no-op.

## Validated producer order

The one real EndTurn macro Resolution emits exactly:

```text
EnergyChanged(3 -> 0)
→ CardZoneChanged(Hand -> Discard) x3
→ StatusChanged(TurnEndDecay 2 -> 1)
→ BlockChanged(Enemy 5 -> 0, TurnStartClear)
→ Damage(Enemy -> Player, Block 7 -> 4)
→ EnergyChanged(0 -> 3)
→ BlockChanged(Player 4 -> 0, TurnStartClear)
→ DeckShuffled(Draw 0 -> 3, Discard 3 -> 0)
→ CardZoneChanged(DrawPile -> Hand) x2
```

There is no `TurnEnded` Presentation Record type and no synthetic turn marker is introduced.

## Macro-resolution boundary

The current ActionQueue contract keeps the authoritative EndTurn -> EnemyTurn -> PlayerTurnStart continuation in one pump frame. QueueEmpty continuations enqueue the next authoritative batch before the queue reaches settled `ResolutionIdle`. Presentation therefore seals only after the complete macro turn returns to `PlayerTurn`.

A2D5-4 validates this as one captured EndTurn Envelope rather than flattening multiple independent Envelopes in test code.

## Controller timing

Gameplay completes the macro turn before presentation playback catches up. The test completes real PlaybackTokens record-by-record and validates historical ViewModel progression:

```text
baseline: Energy3 Hand3 Draw0 Discard0 PBlock7 EBlock5 Decay2
EnergyChanged: Energy0
three discards: Hand3 -> 0, Discard0 -> 3
TurnEndDecay: Decay2 -> 1
Enemy clear: EBlock5 -> 0
Enemy Damage: PBlock7 -> 4
turn-start Energy: Energy0 -> 3
Player clear: PBlock4 -> 0
DeckShuffled: Draw0 -> 3, Discard3 -> 0
first draw: Hand0 -> 1, Draw3 -> 2
second draw: Hand1 -> 2, Draw2 -> 1
```

The drawn RuntimeIds are concrete cards from the original three-card deck and match FinalSnapshot hand order.

## Consistency checks

Validated checks:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No Record sorting is performed before comparison.

## Scope

A2D5-4 changed only Editor Automation tests, CI discovery counts, and documentation. It added no production Record type, no Gameplay turn mechanic, no Controller protocol, and no reducer rule.

Validated discovery counts:

```text
A2D5 focused = 3/3 PASS
Phase6R      = 97/97 PASS
```

## Final status

```text
A2D5-1 VALIDATED
A2D5-2 STATUS LIFECYCLE VALIDATED
A2D5-3 CARD STATUS INTEGRATION VALIDATED
A2D5-4 TURN CYCLE ORDERING VALIDATED
A2D5 FOCUSED 3/3 PASS
PHASE6R 97/97 PASS
SHIPPING EXCLUSION PASS
READY FOR A2D5-5 TERMINAL.VICTORY
```
