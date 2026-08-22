# Phase 6UI-A2D5-7 Terminal.ResolutionFault Review

Date: **2026-08-22**

Status: **IMPLEMENTED / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Branch:

```text
a2d5-terminal-resolution-fault
```

A2D5-6 `Terminal.Defeat` is now validated and sealed. Current validated baseline:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 5/5
Phase6R aggregate               PASS 99/99
Shipping exclusion              PASS
```

After integration, the gate values become:

```text
A2D5 focused expected = 6
Phase6R expected total = 100
```

## Scenario

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.ResolutionFault
```

The test uses the real EndTurn macro flow and the existing ActionQueue structural-failure seam:

```text
SetForceInvalidEnemyTurnBatchForTesting(true)
→ RequestEndPlayerTurn()
```

The test does not call `RequestResolutionFault()` directly. The malformed EnemyTurn batch contains a `TurnEndedAction` with the wrong Outer, so the real atomic batch insertion fails and BattleManager requests the framework fault.

Required visible history:

```text
EnergyChanged(3 -> 0)
→ ResolutionFault
```

The rejected EnemyTurn batch must produce no partial `Damage`, `BlockChanged`, `CardZoneChanged`, `DeckShuffled`, `StatusChanged`, `Victory`, or `Defeat` records.

## Framework fault diagnostics

The terminal Record is matched against ActionQueue-owned diagnostics:

```text
ResolutionFault.Reason == Queue.GetResolutionFaultReason()
ResolutionFault.ExecutedActionCount == Queue.GetExecutedCountInResolution()
ResolutionFault.LastActionName == Queue.GetLastExecutedAction()->GetFName()
```

The last executed action must be the real player `UTurnEndedAction`.

The fault occurs before EnemyTurn state commit:

```text
StateBeforeLastResolutionFaultForTesting = PlayerTurnEnding
Player HP remains 100
Pending actions = 0
BattleState = ResolutionFaulted
```

## Gameplay vs Presentation timing

At publication Gameplay is already `ResolutionFaulted`, but Presentation starts from the historical PlayerTurn baseline.

```text
EnergyChanged completion
    → Working Energy 3 -> 0
    → Outcome remains None

ResolutionFault playback
    → InteractionState = Resolving
    → input locked
    → Outcome = None

ResolutionFault completion
    → Outcome = ResolutionFaulted
    → InteractionState = Terminal
    → exact FinalSnapshot reconciliation
```

Duplicate completion of the same terminal playback token is required to be a NoOp.

## Presentation-failure negative case

The same top-level test also verifies:

```text
Presentation freeze failure != Gameplay ResolutionFault
```

A forced Presentation snapshot freeze failure must result in Presentation unavailability while Gameplay stays in `PlayerTurn` and ActionQueue remains healthy.

## Consistency checks

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No runtime production changes, new Record types, or Controller protocol changes are introduced by A2D5-7.
