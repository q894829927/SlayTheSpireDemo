# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status:

```text
A2D5-1 VALIDATED
A2D5-2 StatusLifecycle VALIDATED
A2D5-3 CardStatusIntegration VALIDATED
A2D5-4 TurnCycleOrdering VALIDATED
A2D5-5 Terminal.Victory VALIDATED
A2D5-6 Terminal.Defeat VALIDATED
A2D5-7 Terminal.ResolutionFault REVIEW FIX APPLIED / UE5.8 REVALIDATION PENDING
```

Current validated baseline after A2D5-6:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
A2D5 focused                    PASS 5/5
Phase6R aggregate               PASS 99/99
Shipping exclusion              PASS
```

The integrated tree contains all six originally planned A2D5 top-level scenarios:

```text
StatusLifecycle
CardStatusIntegration
TurnCycleOrdering
Terminal.Victory
Terminal.Defeat
Terminal.ResolutionFault
```

Current validation targets remain:

```text
A2D5 focused expected = 6
Phase6R expected total = 100
```

`100` is the natural aggregate after the sixth planned A2D5 scenario; no extra test was added to reach a round number.

---

## Shared acceptance architecture

The real acceptance path remains:

```text
Gameplay commit
→ committed Presentation Record
→ immutable Resolution Envelope
→ BattlePresentationController
→ visible record-by-record playback
→ PlaybackToken completion
→ WorkingPresentationSnapshot progression
→ exact FinalSnapshot reconciliation
```

Each Envelope is reduced independently through production reducers. Controller history is compared in producer order without sorting.

Locked contracts remain unchanged:

```text
card cost lives only in CardPlayed
TurnEnded is not a Presentation Record
no committed mutation => no Presentation Record
terminal Record is unique and final
terminal Energy is reconciled by FinalSnapshot
Presentation failure != Gameplay ResolutionFault
```

---

## A2D5-2 — StatusLifecycle

Validated lifecycle:

```text
Weak#A 0 -> 2 Applied
Weak#A 2 -> 3 Increased
Weak#A 3 -> 2 Reduced
Weak#A 2 -> 1 TurnEndDecay
Weak#A 1 -> 0 Removed
Weak#B 0 -> 2 Applied
```

Validated baseline reached **95/95**.

---

## A2D5-3 — CardStatusIntegration

Validated real card history:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak)
→ StatusChanged(Vulnerable)
→ CardZoneChanged
```

Repeated application preserves concrete status identity and RuntimeSequence. Validated baseline reached **96/96**.

---

## A2D5-4 — TurnCycleOrdering

Validated macro-turn history:

```text
EnergyChanged(3 -> 0)
→ Hand -> Discard x3
→ StatusChanged(TurnEndDecay)
→ Enemy BlockChanged(clear)
→ Enemy Damage
→ EnergyChanged(0 -> 3)
→ Player BlockChanged(clear)
→ DeckShuffled
→ DrawPile -> Hand x2
```

Validated baseline reached **97/97**.

---

## A2D5-5 — Terminal.Victory

Validated lethal card history:

```text
CardPlayed
→ Damage(Enemy 100 -> 0)
→ CardZoneChanged(PlayArea -> Discard)
→ Victory
```

Victory is unique/final, lethal state is visible before terminal completion, and duplicate terminal token completion is a NoOp. Validated baseline reached **98/98**.

---

## A2D5-6 — Terminal.Defeat

Validated lethal enemy-turn history:

```text
EnergyChanged(3 -> 0)
→ Damage(EnemyPrimary -> PlayerHero, HP 100 -> 0)
→ Defeat
```

Defeat is unique/final. Player death becomes visible before terminal completion, while Outcome remains `None` and interaction remains `Resolving`. Duplicate terminal token completion is a NoOp.

Validated result:

```text
A2D5 focused 5/5 PASS
Phase6R 99/99 PASS
Shipping exclusion PASS
```

A2D5-6 is sealed.

---

## A2D5-7 — Terminal.ResolutionFault

Top-level test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.ResolutionFault
```

The test uses a genuine framework structural fault rather than Presentation corruption:

```text
SetForceInvalidEnemyTurnBatchForTesting(true)
→ RequestEndPlayerTurn()
→ EnergyChanged(3 -> 0)
→ valid Player TurnEndedAction completes
→ malformed EnemyTurn atomic batch is rejected
→ ActionQueue framework fault
→ ResolutionFault
```

Required visible history:

```text
EnergyChanged(3 -> 0)
→ ResolutionFault
```

The test verifies frozen framework diagnostics by ownership rather than by locking human-readable wording:

```text
Reason is non-empty
Reason == Queue fault reason
ExecutedActionCount == Queue diagnostic
LastActionName == real last executed TurnEndedAction
```

Controller timing requires `Outcome=None / Resolving` until the terminal fault token completes, then `Outcome=ResolutionFaulted / Terminal`, with duplicate token completion as a NoOp.

A second negative fixture proves:

```text
Presentation freeze failure
!=
Gameplay/ActionQueue ResolutionFault
```

### Review defect found by first 6-test run

The first A2D5-7 UE5.8 run reached the Presentation-failure negative fixture and failed only at:

```text
Presentation freeze failure exposes PresentationUnavailable UI
```

Root cause:

```text
PresentationAvailable true -> false
Gameplay BattleId/StateRevision unchanged
TryPublishReadStateReady deduplicated only by BattleId/StateRevision
OnReadStateReady was suppressed
Controller/ViewModel never observed PresentationUnavailable
```

Production fix:

```text
ReadStateReady public-edge identity now includes Presentation availability.
A true -> false availability transition publishes even when Gameplay revision is unchanged.
Repeated identical availability/state edges still deduplicate normally.
```

Files changed for the fix:

```text
Source/SlayTheSpireDemo/Battle/BattleManager.h
Source/SlayTheSpireDemo/Battle/BattleManagerUIA0ReadState.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TerminalResolutionFaultTest.cpp
```

No ActionQueue semantics, terminal reducer, Record taxonomy, or workflow discovery counts were changed.

Current status:

```text
REVIEW FIX APPLIED
STATIC REVIEW COMPLETE
UE5.8 REVALIDATION PENDING
A2D5 focused expected = 6
Phase6R expected total = 100
```
