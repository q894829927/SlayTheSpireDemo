# Phase 6UI-A2D5 Source Review

Date: **2026-08-22**

Status:

```text
A2D5-1 VALIDATED
A2D5-2 StatusLifecycle VALIDATED
A2D5-3 CardStatusIntegration VALIDATED
A2D5-4 TurnCycleOrdering VALIDATED
A2D5-5 Terminal.Victory VALIDATED
A2D5-6 Terminal.Defeat IMPLEMENTED / UE5.8 VALIDATION PENDING
```

Current validated baseline:

```text
UE5.8 Editor Development build   PASS
A2D1                            PASS 3/3
A2D2                            PASS 4/4
A2D3                            PASS 4/4
A2D4                            PASS 6/6
A2D5 focused                    PASS 4/4
Phase6R aggregate               PASS 98/98
Shipping exclusion              PASS
```

The integrated tree now contains five A2D5 top-level tests, so the next validation targets are:

```text
A2D5 focused expected = 5
Phase6R expected total = 99
```

These are expected counts only until `Terminal.Defeat` passes in UE5.8.

---

## Shared acceptance architecture

A2D5 remains a combined C++ acceptance slice. It adds no new Presentation capability merely to satisfy tests.

The real path under acceptance is:

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

Shared support:

```text
Phase6UIA2D5TestTypes.*
Phase6UIA2D5TestSupport.*
Phase6UIA2D5PlaybackAssertions.cpp
```

Each Envelope is checked independently with production reducers. Multiple Resolutions are never flattened into one synthetic reducer history.

Controller playback history is compared in producer order without sorting, including Record and PlaybackToken identity.

---

## Locked cross-slice contracts preserved

### Card cost ownership

Card-play energy cost is represented only by:

```text
CardPlayed.EnergyBefore
CardPlayed.EnergyAfter
CardPlayed.CostPaid
```

No duplicate `EnergyChanged` is emitted for the same card spend.

### Turn boundary

`TurnEnded` is not a Presentation Record. Acceptance verifies the actual visible committed mutations caused by the macro turn.

### No-op rule

No committed mutation means no Presentation Record. Tests do not require records for zero block clears, unchanged energy, empty shuffle sources, stale status mutations, or other no-ops.

### Terminal timing

Terminal Records are unique and final. A terminal Record enters WorkingSnapshot only after its own visible playback completes. Exact terminal reconciliation is then owned by the Envelope FinalSnapshot.

Terminal Energy remains a FinalSnapshot reconciliation field; Victory/Defeat reducers do not synthesize terminal `EnergyChanged`.

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

Coverage includes real pending stale Action identity, exact old-instance isolation, no-record stale NoOp, RuntimeSequence ordering, Controller token order, and per-Envelope reducer consistency.

Validated result:

```text
A2D5 focused 1/1 PASS
Phase6R 95/95 PASS
Shipping PASS
```

---

## A2D5-3 — CardStatusIntegration

Real one-cost card history:

```text
CardPlayed
→ Damage
→ StatusChanged(Weak)
→ StatusChanged(Vulnerable)
→ CardZoneChanged
```

A second runtime card reuses the same concrete Weak/Vulnerable status instances and RuntimeSequences, proving no duplicate status rows are created.

Validated result:

```text
A2D5 focused 2/2 PASS
Phase6R 96/96 PASS
Shipping PASS
```

---

## A2D5-4 — TurnCycleOrdering

The forced macro-turn scenario validates the real order:

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

No `TurnEnded` record exists or is expected.

Validated result:

```text
A2D5 focused 3/3 PASS
Phase6R 97/97 PASS
Shipping PASS
```

---

## A2D5-5 — Terminal.Victory

Real lethal card history:

```text
CardPlayed
→ Damage(Enemy 100 -> 0)
→ CardZoneChanged(PlayArea -> Discard)
→ Victory
```

Victory is unique and final. Enemy is already visibly dead before Victory playback completes, while Outcome remains `None`, interaction remains `Resolving`, and input remains locked.

After the Victory token completes, the ViewModel enters Terminal and reconciles exactly to FinalSnapshot. Re-submitting the same terminal token is a NoOp.

Validated result:

```text
A2D5 focused 4/4 PASS
Phase6R 98/98 PASS
Shipping exclusion PASS
```

A2D5-5 is sealed.

---

## A2D5-6 — Terminal.Defeat

Top-level test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Defeat
```

Real lethal enemy-turn history:

```text
RequestEndPlayerTurn
→ EnergyChanged(3 -> 0)
→ Damage(EnemyPrimary -> PlayerHero, HP 100 -> 0)
→ Defeat
```

Required acceptance:

```text
Defeat unique and final
Winner = EnemyPrimary
Defeated = PlayerHero
Player.bDead visible before terminal completion
Outcome remains None while Defeat playback is active
InteractionState remains Resolving while active
input remains locked
terminal token completes exactly once
duplicate terminal token is NoOp
FinalSnapshot reconciliation exact
```

The fixture intentionally uses an empty Hand and zero Block so no-op cleanup paths cannot add unrelated records.

Static review found no high-confidence production defect and required no A2D1-A2D4 runtime change.

Current status:

```text
IMPLEMENTED
STATIC REVIEW COMPLETE
UE5.8 VALIDATION PENDING
A2D5 focused expected = 5
Phase6R expected total = 99
```

---

## Next after Defeat validation

The final planned A2D5 scenario is:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.ResolutionFault
```

It must use a genuine Gameplay/ActionQueue framework fault, not Presentation corruption.
