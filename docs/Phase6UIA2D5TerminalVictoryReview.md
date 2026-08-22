# Phase 6UI-A2D5-5 Terminal.Victory Review

Date: **2026-08-22**

Status: **IMPLEMENTED / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Validated baseline entering A2D5-5:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 3/3
Phase6R aggregate               PASS 97/97
Shipping exclusion              PASS
```

## Scenario

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Victory
```

File:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TerminalVictoryTest.cpp
```

The fixture uses one real one-cost Enemy-target Attack card:

```text
Damage = 100
Enemy HP = 100
DefaultDestination = Discard
Opening Hand = 1 card
```

No terminal state or Record is fabricated by the test. The card is submitted through the real `ABattleManager::RequestPlayCard()` path.

## Required producer history

The accepted lethal card Resolution must emit exactly:

```text
CardPlayed
→ Damage(Enemy HP 100 -> 0)
→ CardZoneChanged(PlayArea -> DiscardPile)
→ Victory
```

`Victory` must be unique and final.

The test also requires:

```text
Envelope Origin = PlayCard
WinnerPresentationId = PlayerHero
DefeatedPresentationId = EnemyPrimary
no Defeat
no ResolutionFault
no EnergyChanged for the card cost
```

Card cost remains owned only by:

```text
CardPlayed.EnergyBefore = 3
CardPlayed.EnergyAfter = 2
CardPlayed.CostPaid = 1
```

Gameplay terminal normalization later sets authoritative Energy to zero without synthesizing a duplicate `EnergyChanged`.

## Real lethal ordering boundary

The existing runtime queues all card effects before `FinishCardPlayAction`.

After lethal `Damage`, `FinishCardPlayAction` still commits the played card's real destination. `CheckBattleResult()` runs only when the authoritative ActionQueue reaches its empty boundary, so the terminal Record is appended after the card-zone history:

```text
Damage
→ FinishCardPlayAction / CardZoneChanged
→ QueueEmpty / CheckBattleResult
→ Victory
```

This directly validates the locked A2D5 lethal-card contract.

## Gameplay vs Presentation timing

At Envelope publication, authoritative Gameplay is already terminal:

```text
BattleState = Victory
Energy = 0
Enemy HP = 0
Enemy dead = true
Hand = 0
Discard = 1
```

Presentation starts from the pre-Resolution historical baseline:

```text
BattleState = PlayerTurn
Outcome = None
Energy = 3
Enemy HP = 100
Hand = 1
Discard = 0
```

The real Controller then advances token-by-token:

```text
CardPlayed completion
    → Energy 3 -> 2
    → Hand 1 -> 0
    → WorkingSnapshot remains PlayerTurn / Outcome=None

Damage completion
    → Enemy HP 100 -> 0
    → Enemy.bDead = true
    → WorkingSnapshot still PlayerTurn / Outcome=None

CardZoneChanged completion
    → Discard 0 -> 1
    → Victory playback begins
    → WorkingSnapshot still PlayerTurn / Outcome=None

Victory completion
    → terminal Record is reduced
    → Envelope immediately reconciles to exact FinalSnapshot
    → ViewModel Outcome = Victory
    → ViewModel InteractionState = Terminal
    → Energy reconciles to terminal authoritative zero
    → caught-up Controller releases its WorkingSnapshot
```

The pre-terminal WorkingSnapshot therefore proves that lethal damage and card cleanup are already visible while terminal state is still withheld.

## Terminal input lock

While Victory playback is active, the displayed HUD must remain:

```text
InteractionState = Resolving
bInputLocked = true
bCanEndTurn = false
Outcome = None
```

Only after the terminal token completes does it enter:

```text
InteractionState = Terminal
Outcome = Victory
bInputLocked = true
bCanEndTurn = false
```

## Token exactly-once behavior

The test captures the real active Victory `FPresentationPlaybackToken` and verifies:

```text
BattleId matches Victory Record
ResolutionId matches Victory Record
PresentationSequence matches Victory Record
LocalPlaybackGeneration > 0
```

After normal Victory completion, the same token is submitted a second time.

Required result:

```text
no new playback call
no new wait state
LastCompletedResolutionId unchanged
Outcome remains Victory
```

This proves duplicate/stale terminal completion cannot advance the Controller twice.

## Consistency checks

The scenario also requires:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No Record sorting is performed before comparison.

Terminal Energy ownership remains intentionally special: the terminal reducer owns only `BattleState`, `Outcome`, and `bCanEndTurn`; exact terminal Energy is supplied by FinalSnapshot reconciliation.

## Static review result

No high-confidence cross-slice production defect was found.

A2D5-5 changes only:

```text
Editor Automation test
A2D5 focused discovery count
Phase6R discovery count
documentation
```

It adds no Presentation Record type, no Gameplay terminal mechanic, no Controller protocol, and no reducer ownership rule.

Focused gate now expects:

```text
A2D5 = 4 tests
```

Updated Phase6R expected discovery total:

```text
98
```

These are expected counts only until UE5.8 focused and aggregate workflows pass.
