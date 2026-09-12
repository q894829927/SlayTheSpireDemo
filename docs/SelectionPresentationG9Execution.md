# Selection Presentation G9 — Execution

Date: **2026-09-13**

Status: **G9-A IN PROGRESS — BUILD PASS / FOCUSED AUTOMATION PASS / G8 REGRESSIONS PENDING**

Design authority: [`SelectionPresentationG9Design.md`](SelectionPresentationG9Design.md).
Baseline authority: [`SelectionPresentationG8FSeal.md`](SelectionPresentationG8FSeal.md).

This document tracks implementation/validation status only. The design document remains normative for behavior.

---

## G9-A — Authority Foundation (shadow only)

### A1 — Gameplay player-turn authority

Implemented on `g9/g9-a-buffered-player-intent-foundation`:

```text
FPlayerTurnAuthorityToken { BattleId, PlayerTurnSerial }
PlayerTurnSerial reset on StartBattle
PlayerTurnSerial increment exactly once on formal CompletePlayerTurnStart
TryGetCurrentPlayerTurnAuthorityToken
OnPlayerCommandOpportunity emitted from authoritative ResolutionIdle
```

Important boundary:

```text
ResolutionIdle wake-up is independent from ReadStateReady de-duplication.
```

### A2 — shadow buffered EndTurn / arbitration

Implemented:

```text
FBufferedEndTurnIntent
FBufferedCardIntent storage shape
single shadow pending-intent owner
EndTurn > CardSelection arbitration
EndTurn capture/evaluation: Ready / Waiting(ResolutionBusy) / Stale
mandatory-selection fencing
no Gameplay replay
```

Focused tests added:

```text
SlayTheSpireDemo.SelectionPresentation.G9A.Authority.PlayerTurnABA
SlayTheSpireDemo.SelectionPresentation.G9A.EndTurn.ResolutionIdleOpportunity
SlayTheSpireDemo.SelectionPresentation.G9A.Intent.Arbitration
SlayTheSpireDemo.SelectionPresentation.G9A.EndTurn.MandatorySelectionFence
```

### A3 — exact sealed card target authority

Implemented:

```text
UBattlePresentationController::TryCaptureBufferedCardTarget
UBattlePresentationController::EvaluateBufferedCardTarget
```

Capture is intentionally narrow:

```text
PresentationOwned only
exact current PresentationSessionToken
single-record Blocking playback only
approved window only:
  CardPlayed
  PlayArea -> DiscardPile
  PlayArea -> ExhaustPile
  PlayArea -> RemovedPile
latest frozen baseline already sealed
exact target BattleId + StateRevision
BattleState == PlayerTurn
Outcome == None
no authoritative PendingCardSelection
target RuntimeId survives from displayed Hand into sealed target Hand
target frozen card is gameplay-playable
```

Explicitly forbidden:

```text
Draw/Shuffle/Block/Status as card-buffer anchors
CurrentRevision + 1 prediction
numeric revision range authority
Gameplay-busy speculative future card surfaces
DirectBaseline card buffering
```

Focused A3 tests added:

```text
SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.ExactWaitReady
SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.ApprovedWindowOnly
SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.SealedTargetChangeStales
SlayTheSpireDemo.SelectionPresentation.G9A.CardTarget.SessionReplacementStales
```

### Validation checkpoint

Reported by user:

```text
UE5.8 Development Editor build with A3 — PASS
SlayTheSpireDemo.SelectionPresentation.G9A — PASS
```

The focused suite contains the four A1/A2 tests plus the four A3 tests above. No G9 production replay/input behavior is enabled by this checkpoint.

---

## Current validation gates

Before G9-A can become COMPLETE / VALIDATED:

```text
1. UE5.8 Development Editor build with A3 — PASS
2. SlayTheSpireDemo.SelectionPresentation.G9A — PASS
3. affected G8 regressions — pending
4. static review: no production input behavior changed — pending final checkpoint
```

Minimum affected regressions:

```text
SlayTheSpireDemo.SelectionPresentation.G8B
SlayTheSpireDemo.Phase6UIA2N.FastInput
SlayTheSpireDemo.SelectionPresentation.G8A
SlayTheSpireDemo.SelectionPresentation.G8D
SlayTheSpireDemo.CardSelection.Unified
```

G9-B remains **NOT STARTED**. G9-A does not alter hover, EndTurn UI availability, ChoosingTarget behavior, FastInput Skip, or card visual ownership.
