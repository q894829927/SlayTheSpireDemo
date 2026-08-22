# Phase 6UI-A2D5-7 Terminal.ResolutionFault Review

Date: **2026-08-22**

Status: **VALIDATED / SEALED**.

A2D5-6 `Terminal.Defeat` was already validated and sealed. The owner reran the A2D5 focused gate after the Presentation-availability review fix and confirmed success, then completed the expanded aggregate + Shipping closure.

Final owner-confirmed evidence:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 6/6
A2D5-7 Terminal.ResolutionFault PASS
Phase6R aggregate               PASS 100/100
Shipping exclusion              PASS
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

The rejected EnemyTurn batch produces no partial `Damage`, `BlockChanged`, `CardZoneChanged`, `DeckShuffled`, `StatusChanged`, `Victory`, or `Defeat` records.

## Framework fault diagnostics

The terminal Record is matched against ActionQueue-owned diagnostics:

```text
ResolutionFault.Reason is non-empty
ResolutionFault.Reason == Queue.GetResolutionFaultReason()
ResolutionFault.ExecutedActionCount == Queue.GetExecutedCountInResolution()
ResolutionFault.LastActionName == Queue.GetLastExecutedAction()->GetFName()
```

The human-readable English reason is deliberately not treated as a stable ABI. The last executed action must be the real player `UTurnEndedAction`.

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

Duplicate completion of the same terminal playback token is a NoOp.

## Presentation-failure negative case

The same top-level test verifies:

```text
Presentation freeze failure != Gameplay ResolutionFault
```

A forced Presentation snapshot freeze failure results in Presentation unavailability while Gameplay stays in `PlayerTurn` and ActionQueue remains healthy.

### Defect found in first UE5.8 six-test run

The first run failed only at:

```text
Presentation freeze failure exposes PresentationUnavailable UI
```

The underlying Gameplay state was correct and Presentation was marked unavailable, but the public read edge was suppressed because the old dedupe key considered only:

```text
BattleId
StateRevision
```

Freeze failure changes Presentation availability without mutating Gameplay revision, so Controller/ViewModel originally received no `OnReadStateReady` notification.

Production fix:

```text
ReadStateReady dedupe also tracks the last published Presentation availability.
```

Therefore:

```text
same BattleId
same StateRevision
PresentationAvailable true -> false
```

still produces one public read edge. Once that unavailable state has been published, identical later edges deduplicate again.

This preserves the ownership boundary:

```text
Presentation failure
→ PresentationUnavailable UI
→ input locked

but

Gameplay remains PlayerTurn
ActionQueue remains healthy
Outcome remains None
```

The owner-confirmed rerun passed the complete focused gate after this fix, and the final 100/100 aggregate plus Shipping gate also passed.

## Consistency checks

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

The review fix changed only read-edge availability identity plus test diagnostic matching. It did not change ActionQueue semantics, terminal reducer behavior, Record taxonomy, Controller token protocol, or test discovery counts.

## Closure / next step

A2D5 C++/Automation acceptance is complete and sealed.

Proceed to:

```text
UI-A2E — Unified Blueprint Playback & PIE Acceptance
```

See `docs/Phase6UIA2EImplementation.md` for the locked A2E -> A3 sequence.
