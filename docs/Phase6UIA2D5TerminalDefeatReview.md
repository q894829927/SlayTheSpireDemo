# Phase 6UI-A2D5-6 Terminal.Defeat Review

Date: **2026-08-22**

Status: **VALIDATED / SEALED**.

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.Defeat
```

Validated UE5.8 baseline after A2D5-6:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 5/5
Phase6R aggregate               PASS 99/99
Shipping exclusion              PASS
```

The scenario uses the real committed enemy intent path:

```text
Player HP = 100
Player Block = 0
Player Energy = 3
Hand = empty
Enemy committed Attack = 100
```

Formal request:

```text
ABattleManager::RequestEndPlayerTurn()
```

Validated producer history:

```text
EnergyChanged(3 -> 0)
→ Damage(EnemyPrimary -> PlayerHero, HP 100 -> 0)
→ Defeat
```

Validated terminal rules:

```text
Envelope Origin = EndTurn
Defeat unique and final
Winner = EnemyPrimary
Defeated = PlayerHero
no Victory
no ResolutionFault
no BlockChanged for zero-block clears
no CardZoneChanged for empty Hand
no DeckShuffled after terminal enemy damage
```

Controller timing was validated record-by-record:

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

While Defeat playback is active:

```text
InteractionState = Resolving
Outcome = None
bInputLocked = true
bCanEndTurn = false
```

The real Defeat token is captured and duplicate completion is verified as a NoOp.

Consistency coverage:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No production runtime changes were required.

A2D5-6 is sealed. The next and final planned A2D5 top-level scenario is `Terminal.ResolutionFault`.
