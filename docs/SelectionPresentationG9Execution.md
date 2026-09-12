# Selection Presentation G9 — Execution

Date: **2026-09-13**

Status: **G9-A COMPLETE / VALIDATED — G9-B IN PROGRESS / BUILD PENDING**

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

Focused tests:

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

Focused A3 tests:

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

Affected G8 regressions — PASS:
SlayTheSpireDemo.SelectionPresentation.G8B
SlayTheSpireDemo.Phase6UIA2N.FastInput
SlayTheSpireDemo.SelectionPresentation.G8A
SlayTheSpireDemo.SelectionPresentation.G8D
SlayTheSpireDemo.CardSelection.Unified
```

The focused suite contains the four A1/A2 tests plus the four A3 tests above.

Static review against the G9-A branch point also passes:

```text
no SelectCard production path change
no EndTurn production path change
no FastInput production path change
no Hand hover/layout production path change
no card visual ownership production path change
```

Result:

```text
G9-A — COMPLETE / VALIDATED
```

---

## G9-B — Buffered Player Input + Hand Hover

Implementation branch:

```text
g9/g9-b-buffered-player-input-hand-hover
```

### B1 — Buffered CardSelection + hover/layout split

Implemented, validation pending:

```text
exact G9 card-buffer capture is attempted before sealed G8 FastInput Skip
exact capture success:
  store one BufferedCardSelection
  do NOT Skip current played-card Presentation
  replay normal SelectCard only after exact target becomes Ready
capture failure:
  retire older card credential
  preserve ordinary G8 FastInput/reject fallback for the new click
```

Hand behavior:

```text
structural fan layout remains frozen during active played-card Presentation
hover affordance is a separate UpdateHoverAffordance path
hover-only path never calls LayoutCards
hover is enabled only for approved G9 card windows:
  CardPlayed
  PlayArea -> destination
G9 disabled -> sealed pre-G9 hover behavior remains
```

### B2 — Buffered EndTurn production activation

Implemented, validation pending:

```text
single ViewModel-side pending G9 intent owner
BufferedEndTurn > BufferedCardSelection > pending G8 FastInput retry
CanAcceptEndTurnIntentG9 is independent from generic bInputLocked/bCanEndTurn
DirectBaseline and PresentationOwned both use Gameplay PlayerTurnAuthorityToken
accepted EndTurn retires buffered card before future replay
accepted EndTurn creates a retirement fence for any older G8 FastInput retry
ReadyToConfirm / ChoosingTarget are cancelled only AFTER exact EndTurn acceptance
mandatory authoritative selection remains a hard rejection/stale boundary
```

ResolutionBusy replay:

```text
ActionQueue ResolutionIdle
→ OnPlayerCommandOpportunity
→ schedule one next-tick exact revalidation
→ never RequestEndPlayerTurn from inside ResolutionIdle callback stack
→ RequestEndPlayerTurn exactly once if the same turn token is still Ready
```

Native EndTurn button:

```text
G9 enabled:
  enabled by CanAcceptEndTurnIntentG9
  physical click routes through exact G9 EndTurn transaction

G9 disabled:
  proxy falls back to existing EndTurn()/RequestEndTurn behavior
  old bInputLocked + bCanEndTurn availability semantics are restored
```

### B focused Automation added

```text
SlayTheSpireDemo.SelectionPresentation.G9B.Card.BufferedReplayExactOnce
SlayTheSpireDemo.SelectionPresentation.G9B.Arbitration.EndTurnOverridesCard
SlayTheSpireDemo.SelectionPresentation.G9B.EndTurn.DirectBaselineResolutionBusy
SlayTheSpireDemo.SelectionPresentation.G9B.EndTurn.ReadyToConfirmSupersede
SlayTheSpireDemo.SelectionPresentation.G9B.EndTurn.ChoosingTargetSupersede
SlayTheSpireDemo.SelectionPresentation.G9B.Hover.LayoutIsolation
```

### Current B gate

```text
UE5.8 Development Editor build           PENDING
G9-B focused Automation                  PENDING
G9-A regression                          PENDING
G8 affected regressions                  PENDING
manual PIE card hover/buffer/EndTurn     PENDING
```

G9-C remains **NOT STARTED**. G9-B still does not detach CardPlayed or destination visual ownership; those timing changes remain G9-C/G9-D work.
