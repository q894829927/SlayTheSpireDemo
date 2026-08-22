# Phase 6UI-A2D5-6 Terminal.Defeat Review

Date: **2026-08-22**

Status: **IMPLEMENTED / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Integration branch:

```text
a2d5-terminal-defeat
```

Validated baseline entering A2D5-6:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 4/4
Phase6R aggregate               PASS 98/98
Shipping exclusion              PASS
```

A2D5-5 `Terminal.Victory` is validated and sealed.

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

The real formal request is:

```text
ABattleManager::RequestEndPlayerTurn()
```

No terminal state or terminal Record is fabricated.

Required producer history:

```text
EnergyChanged(3 -> 0)
→ Damage(EnemyPrimary -> PlayerHero, HP 100 -> 0)
→ Defeat
```

Required terminal rules:

```text
Envelope Origin = EndTurn
Defeat is unique and final
Winner = EnemyPrimary
Defeated = PlayerHero
no Victory
no ResolutionFault
no BlockChanged for zero-block clears
no CardZoneChanged for empty Hand
no DeckShuffled after terminal enemy damage
```

The absence assertions are mutation-driven: zero Block and empty Hand are legal no-ops and must not synthesize records.

Runtime ordering basis:

```text
RequestEndPlayerTurn
→ Energy clear
→ StartEnemyTurn
→ committed enemy Damage
→ Player becomes dead
→ Enemy TurnEndedAction skips event dispatch because a combatant is dead
→ QueueEmpty / CheckBattleResult
→ Defeat
```

The flow does not progress into another PlayerTurn after lethal enemy damage.

Controller timing under test:

```text
EnergyChanged completion
    → Energy 3 -> 0
    → WorkingSnapshot remains non-terminal

Damage completion
    → Player HP 100 -> 0
    → Player.bDead = true
    → Defeat playback begins
    → WorkingSnapshot still PlayerTurn / Outcome=None

Defeat completion
    → ViewModel Outcome = Defeat
    → InteractionState = Terminal
    → exact FinalSnapshot reconciliation
    → caught-up Controller releases WorkingSnapshot
```

While Defeat is active:

```text
InteractionState = Resolving
Outcome = None
bInputLocked = true
bCanEndTurn = false
```

The real Defeat token is captured and a duplicate completion is required to be a NoOp.

Consistency coverage:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No production runtime changes were required by the static review.

After integration the focused gate expects **5** A2D5 tests and Phase6R expects **99** total tests. These remain expected counts until A2D5-6 validation passes.
