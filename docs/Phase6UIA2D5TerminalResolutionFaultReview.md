# Phase 6UI-A2D5-7 Terminal.ResolutionFault Review

Date: **2026-08-22**

Status: **IMPLEMENTED ON ISOLATED BRANCH / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

Branch:

```text
a2d5-terminal-resolution-fault
```

This branch was created from the current `main` while A2D5-6 `Terminal.Defeat` validation was running. The in-flight `main` workflow therefore remains unchanged at:

```text
A2D5 focused expected = 5
Phase6R expected total = 99
```

The last user-confirmed aggregate baseline entering this work is:

```text
UE5.8 Editor Development build   PASS
A2D5 focused                    PASS 4/4
Phase6R aggregate               PASS 98/98
Shipping exclusion              PASS
```

A2D5-6 `Terminal.Defeat` is present on `main` but is still pending the user's current 5/5 and 99/99 validation result. This document does not claim those results before they are reported.

## Scenario

Top-level Automation test:

```text
SlayTheSpireDemo.Phase6UIA2D5.Terminal.ResolutionFault
```

File:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2D5TerminalResolutionFaultTest.cpp
```

The test uses a real EndTurn macro flow and the existing ActionQueue structural-failure seam:

```text
SetForceInvalidEnemyTurnBatchForTesting(true)
→ RequestEndPlayerTurn()
```

The seam does not call `RequestResolutionFault()` directly from the test. Instead, it gives the EnemyTurn `TurnEndedAction` an invalid Outer. `StartEnemyTurn()` then attempts the real atomic batch insertion, which fails validation and causes BattleManager to request the framework fault.

No Presentation history is corrupted or fabricated to create the terminal state.

## Required producer history

The fixture deliberately starts with:

```text
Player Energy = 3
Player HP = 100
Hand = empty
Player Block = 0
Enemy Block = 0
no Statuses
```

The accepted EndTurn Resolution therefore commits one ordinary visible fact before the framework failure:

```text
EnergyChanged(3 -> 0)
→ ResolutionFault
```

The test requires exactly two Records and explicitly rejects:

```text
Damage
BlockChanged
CardZoneChanged
DeckShuffled
StatusChanged
Victory
Defeat
```

The EnemyTurn `DamageAction` is constructed as part of the malformed batch, but atomic insertion rejection guarantees that it never executes.

## Genuine framework fault boundary

The authoritative failure chain is:

```text
RequestEndPlayerTurn
→ valid Player TurnEndedAction executes
→ QueueEmpty deferred continuation enters StartEnemyTurn
→ EnemyTurn batch contains invalid-Outer TurnEndedAction
→ AddBatchToBackPreserveOrder rejects whole batch
→ BattleManager calls RequestResolutionFault
→ ActionQueue enters ResolutionFault at safe point
→ BattleManager commits BattleState = ResolutionFaulted
→ ResolutionFault Presentation Record appended
```

The test requires:

```text
StateBeforeLastResolutionFaultForTesting = PlayerTurnEnding
Queue IsResolutionFaulted = true
BattleState = ResolutionFaulted
Energy = 0
Player HP remains 100
Pending actions = 0
```

This proves the fault happens before EnemyTurn state commit and before any partial EnemyTurn action can execute.

## Frozen framework diagnostics

The terminal Record is compared directly against the ActionQueue's authoritative diagnostics:

```text
ResolutionFault.Reason
    == Queue.GetResolutionFaultReason()
    == "Enemy turn batch insertion failed before EnemyTurn state commit."

ResolutionFault.ExecutedActionCount
    == Queue.GetExecutedCountInResolution()

ResolutionFault.LastActionName
    == Queue.GetLastExecutedAction()->GetFName()
```

The last executed Action is also required to be the real player `UTurnEndedAction`.

The test intentionally compares against runtime-owned diagnostics instead of hard-coding a generated UObject instance name.

## Gameplay vs Presentation timing

At Envelope publication, authoritative Gameplay is already faulted:

```text
BattleState = ResolutionFaulted
Energy = 0
Player HP = 100
```

Presentation still begins at the pre-Resolution historical baseline:

```text
BattleState = PlayerTurn
Outcome = None
Energy = 3
Player HP = 100
```

Real Controller playback proceeds:

```text
EnergyChanged completion
    → Working Energy 3 -> 0
    → WorkingSnapshot remains PlayerTurn / Outcome=None

ResolutionFault playback begins
    → ViewModel remains Resolving
    → input remains locked
    → Outcome remains None

ResolutionFault completion
    → terminal reducer commits ResolutionFaulted
    → Envelope reconciles to exact FinalSnapshot
    → ViewModel Outcome = ResolutionFaulted
    → InteractionState = Terminal
    → caught-up Controller releases WorkingSnapshot
```

The active terminal token is captured and verified by BattleId, ResolutionId, PresentationSequence and positive LocalPlaybackGeneration.

After normal completion, resubmitting the same token must be a no-op:

```text
LastCompletedResolutionId unchanged
PlayCallCount unchanged
no wait state restored
Outcome remains ResolutionFaulted
```

## Per-Envelope consistency

The scenario retains the shared A2D5 checks:

```text
AssertReducerOwnedStateMatchesFinalSnapshot()
AssertCapturedEnvelopeOrder()
AssertControllerPlaybackMatchesCapturedHistory()
```

No Record sorting or alternate reducer implementation is introduced.

## Presentation-failure negative assertion

The same top-level acceptance test uses a second real fixture to preserve the locked ownership boundary:

```text
Presentation failure != Gameplay ResolutionFault
```

The negative fixture:

```text
BeginSystemPresentationResolutionForTesting()
→ SetForcePresentationFreezeFailureForTesting(true)
→ SealActivePresentationResolutionForTesting()
```

Expected result:

```text
seal fails
PresentationAvailable = false
Gameplay BattleState remains PlayerTurn
ActionQueue IsResolutionFaulted = false
no ResolutionFault Envelope
no playback Record
ViewModel InteractionState = PresentationUnavailable
ViewModel Outcome = None
```

This directly distinguishes Presentation availability failure from a genuine ActionQueue/Gameplay framework fault.

## Static review result

No high-confidence production runtime defect was found.

Current branch changes are limited to:

```text
Editor Automation test
A2D5-7 review documentation
```

No runtime code, Record taxonomy, Controller protocol, Gameplay mechanic, or workflow discovery count has been changed while A2D5-6 validation is in flight.

After A2D5-6 is confirmed and this branch is integrated, the planned gate values become:

```text
A2D5 focused expected = 6
Phase6R expected total = 100
```

This reaches 100 because all six originally planned A2D5 top-level scenarios are then present; no extra test is added merely to reach a round total.
