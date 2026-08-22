# Phase 6UI-A2D5-6 Terminal.Defeat Review

Date: **2026-08-22**

Status: **IMPLEMENTED ON ISOLATED BRANCH / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Branch:

```text
a2d5-terminal-defeat
```

The branch was created from the current `main` while A2D5-5 validation was running so the in-flight focused/Phase6R workflows remain pinned to the existing A2D5 discovery count.

Validated baseline entering A2D5-6 remains the last user-confirmed baseline:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 3/3
Phase6R aggregate               PASS 97/97
Shipping exclusion              PASS
```

A2D5-5 Terminal.Victory is present on the branch but is still pending the user's current UE5.8 validation result. Therefore this document does not claim 98/98 or 99/99 PASS.

## Scenario

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Defeat
```

File:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TerminalDefeatTest.cpp
```

The fixture uses the real committed enemy intent path:

```text
Player HP = 100
Player Block = 0
Player Energy = 3
Hand = empty
Enemy committed Attack = 100
```

The test submits the real formal request:

```text
ABattleManager::RequestEndPlayerTurn()
```

No terminal state or terminal Record is fabricated by the test.

## Required producer history

With an empty Hand, no statuses, and zero Block on both sides, the real EndTurn macro Resolution must emit exactly:

```text
EnergyChanged(3 -> 0)
→ Damage(EnemyPrimary -> PlayerHero, HP 100 -> 0)
→ Defeat
```

The test requires:

```text
Envelope Origin = EndTurn
Defeat is unique
Defeat is final
WinnerPresentationId = EnemyPrimary
DefeatedPresentationId = PlayerHero
no Victory
no ResolutionFault
no BlockChanged for zero-block clears
no CardZoneChanged for empty Hand
no DeckShuffled after terminal enemy damage
```

The absence assertions are mutation-driven: zero Block and empty Hand are legal no-ops and must not synthesize Presentation Records.

## Runtime ordering basis

`RequestEndPlayerTurn()` commits the EndTurn Presentation Resolution and appends `EnergyChanged` only when Energy actually changes.

The authoritative macro flow then reaches `StartEnemyTurn()`, which constructs the committed enemy Attack action. With `EnemyTestAttackDamage = 100`, the real `DamageAction` kills the 100-HP Player.

The queued Enemy `TurnEndedAction` sees that a combatant is already dead and skips turn-end event dispatch. When the ActionQueue reaches its empty boundary, `CheckBattleResult()` observes the dead Player, commits `BattleState = Defeat`, normalizes Energy to zero, and appends the typed `Defeat` terminal Record.

The complete visible history therefore remains in the same real EndTurn macro Resolution and does not progress into another PlayerTurn.

## Gameplay vs Presentation timing

At Envelope publication, authoritative Gameplay is already terminal:

```text
BattleState = Defeat
Energy = 0
Player HP = 0
Player dead = true
```

Presentation starts from the pre-Resolution historical baseline:

```text
BattleState = PlayerTurn
Outcome = None
Energy = 3
Player HP = 100
Player dead = false
```

The real Controller advances token-by-token:

```text
EnergyChanged completion
    → Energy 3 -> 0
    → WorkingSnapshot remains PlayerTurn / Outcome=None

Damage completion
    → Player HP 100 -> 0
    → Player.bDead = true
    → Defeat playback begins
    → WorkingSnapshot still PlayerTurn / Outcome=None

Defeat completion
    → terminal Record is reduced
    → Envelope immediately reconciles to exact FinalSnapshot
    → ViewModel Outcome = Defeat
    → ViewModel InteractionState = Terminal
    → caught-up Controller releases its WorkingSnapshot
```

This proves the Damage reducer has already made the Player visibly dead before terminal state is exposed.

## Terminal input lock

While Defeat playback is active, the displayed HUD must remain:

```text
InteractionState = Resolving
Outcome = None
bInputLocked = true
bCanEndTurn = false
```

Only after the terminal token completes does it enter:

```text
InteractionState = Terminal
Outcome = Defeat
bInputLocked = true
bCanEndTurn = false
```

## Token exactly-once behavior

The test captures the real active Defeat `FPresentationPlaybackToken` and verifies:

```text
BattleId matches Defeat Record
ResolutionId matches Defeat Record
PresentationSequence matches Defeat Record
LocalPlaybackGeneration > 0
```

After normal Defeat completion, the same token is submitted again.

Required result:

```text
no new playback call
no new wait state
LastCompletedResolutionId unchanged
Outcome remains Defeat
```

## Consistency checks

The scenario also requires:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No Record sorting is performed before comparison.

## Static review result

No high-confidence cross-slice production defect was found.

A2D5-6 currently changes only the isolated test branch:

```text
Editor Automation test
documentation
```

The `main` branch workflows are intentionally not changed while A2D5-5 validation is in flight. After that validation result is known, integration can update the focused discovery count from 4 to 5 and the Phase6R expected total from 98 to 99 in one controlled step.
