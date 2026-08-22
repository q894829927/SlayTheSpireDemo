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
A2D5-7 Terminal.ResolutionFault VALIDATED
```

Owner-confirmed current focused evidence:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
A2D5 focused                    PASS 6/6
```

The last separately confirmed aggregate baseline before A2D5-7 was:

```text
Phase6R aggregate               PASS 99/99
Shipping exclusion              PASS
```

The workflow now expects the natural final aggregate of `100` tests after all six planned A2D5 top-level scenarios. Do **not** record `Phase6R 100/100` or the corresponding Shipping result as passed until that separate owner run is explicitly confirmed.

The integrated tree contains exactly the six originally planned A2D5 top-level scenarios:

```text
StatusLifecycle
CardStatusIntegration
TurnCycleOrdering
Terminal.Victory
Terminal.Defeat
Terminal.ResolutionFault
```

No seventh test was added merely to reach a round aggregate number.

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

Stale exact-instance actions cannot retarget the recreated status instance. RuntimeSequence identity/order and frozen status metadata remain part of the acceptance contract.

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

Repeated application produces `Increased` changes on the same concrete runtime identities rather than duplicate status rows.

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

Producer order is actual Gameplay order and is not sorted for Presentation convenience.

---

## A2D5-5 — Terminal.Victory

Validated lethal card history:

```text
CardPlayed
→ Damage(Enemy 100 -> 0)
→ CardZoneChanged(PlayArea -> Discard)
→ Victory
```

Victory is unique/final, lethal state is visible before terminal completion, and duplicate terminal-token completion is a NoOp.

---

## A2D5-6 — Terminal.Defeat

Validated lethal enemy-turn history:

```text
EnergyChanged(3 -> 0)
→ Damage(EnemyPrimary -> PlayerHero, HP 100 -> 0)
→ Defeat
```

Defeat is unique/final. Player death becomes visible before terminal completion, while Outcome remains `None` and interaction remains `Resolving`. Duplicate terminal-token completion is a NoOp.

Previous owner-confirmed aggregate evidence at this point was:

```text
A2D5 focused 5/5 PASS
Phase6R 99/99 PASS
Shipping exclusion PASS
```

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

### Review defect and production fix

The first six-test run failed only because a `PresentationAvailable true -> false` transition with unchanged Gameplay `(BattleId, StateRevision)` was suppressed by read-edge de-duplication. That prevented Controller/ViewModel from observing `PresentationUnavailable`.

The production read-edge identity was corrected so Presentation availability participates in de-duplication:

```text
same BattleId
+ same StateRevision
+ same Presentation availability
→ duplicate edge may be suppressed

same BattleId
+ same StateRevision
+ availability changed
→ publish a new public read edge
```

This preserves the ownership split:

```text
Presentation failure
→ PresentationUnavailable / input locked
→ Gameplay remains healthy
→ no fake ResolutionFault
```

The owner reran the focused A2D5 gate after the fix and confirmed success:

```text
A2D5 focused 6/6 PASS
A2D5-7 Terminal.ResolutionFault VALIDATED
```

No ActionQueue semantics, terminal reducer, Record taxonomy, or test-discovery counts were changed by the review fix.

---

## Next gate and roadmap

A2D5 focused acceptance is complete. Before declaring the entire A2 C++ gate sealed, record the separate aggregate evidence when available:

```text
Phase6R expected = 100/100
Shipping exclusion expected = PASS
```

After aggregate closure, the next implementation stage is **UI-A2E — Unified Blueprint Playback & PIE Acceptance**, not unfinished A3 Preview work. The locked follow-up route is recorded in:

```text
docs/Phase6UIA2EImplementation.md
```

The key ordering is:

```text
A2D5 closure
→ A2E unified Blueprint/UMG playback
→ A2E PIE end-to-end acceptance
→ UI-A2 COMPLETE / SEALED
→ A3-1 Dynamic Text sealed
→ A3-2 Target-Specific Current-State Preview
→ A3-3 Energy + Target-Aware Legality
→ A3-4 ViewModel transient Preview lifecycle
→ A3-5 minimal UMG + A2/A3 combined PIE
```
