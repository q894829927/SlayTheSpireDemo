# Phase 6UI-A2D5-5 Terminal.Victory Review

Date: **2026-08-22**

Status: **VALIDATED / SEALED**.

Validated result reported after implementation:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 4/4
Phase6R aggregate               PASS 98/98
Shipping exclusion              PASS
```

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Victory
```

File:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TerminalVictoryTest.cpp
```

The scenario uses one real one-cost Enemy-target Attack card:

```text
Damage = 100
Enemy HP = 100
DefaultDestination = Discard
Opening Hand = 1 card
```

The accepted lethal card Resolution validates the real producer history:

```text
CardPlayed
→ Damage(Enemy HP 100 -> 0)
→ CardZoneChanged(PlayArea -> DiscardPile)
→ Victory
```

Validated terminal rules:

```text
Victory is unique and final
Winner = PlayerHero
Defeated = EnemyPrimary
no Defeat
no ResolutionFault
no duplicate EnergyChanged for card cost
```

Card cost remains owned by:

```text
CardPlayed.EnergyBefore = 3
CardPlayed.EnergyAfter = 2
CardPlayed.CostPaid = 1
```

Gameplay terminal normalization sets authoritative Energy to zero later without synthesizing a duplicate card-cost record.

Controller timing is validated token-by-token:

```text
CardPlayed completion
    → Energy 3 -> 2
    → Hand 1 -> 0

Damage completion
    → Enemy HP 100 -> 0
    → Enemy.bDead = true
    → Outcome still None

CardZoneChanged completion
    → Discard 0 -> 1
    → Victory playback begins
    → WorkingSnapshot still PlayerTurn / Outcome=None

Victory completion
    → ViewModel Outcome = Victory
    → InteractionState = Terminal
    → exact FinalSnapshot reconciliation
    → terminal Energy = 0
    → caught-up Controller releases WorkingSnapshot
```

While Victory is active the HUD remains:

```text
InteractionState = Resolving
Outcome = None
bInputLocked = true
bCanEndTurn = false
```

The test also captures the real Victory `FPresentationPlaybackToken`, completes it once, then submits the same token again and proves the duplicate completion is a NoOp.

Consistency coverage remains:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No production runtime changes were required by A2D5-5.

A2D5-5 is sealed. The next acceptance scenario is `Terminal.Defeat`.
