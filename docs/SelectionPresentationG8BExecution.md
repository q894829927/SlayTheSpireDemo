# Selection Presentation G8-B Execution

Date: 2026-09-11

Status: **IMPLEMENTED ON MAIN / BUILD NOT RUN / AUTOMATION NOT RUN / NOT VALIDATED**.

G8-B implements the readiness, exact PendingSelection identity, and FastInput shadow contracts defined by `docs/SelectionPresentationG8Design.md`. It does not enable G8-C detached Damage production, compatibility debt, or earlier Gameplay unlock.

Implementation base:

```text
a52f2c8537c720589be1669963e01a4137127f33
G8-A validated checkpoint
```

G8-B runtime implementation checkpoint before this documentation commit:

```text
5bb6998ce7e88992557b1bcfc9cea7b42e5783f4
```

## Implemented scope

### Prerequisite baseline fixes

- `ChoosingTarget` now explicitly disables `bCanEndTurn`.
- `RequestEndTurn()` rejects while `ChoosingTarget` without clearing the selected card/target-choice surface.
- Selecting another legal Hand card while `ChoosingTarget` remains allowed and rebuilds the new card's target surface.
- Controller initialization failure remains the G8-A `PresentationUnavailable` fail-safe; G8-B does not restore DirectBaseline fallback.

### Exact PendingSelection identity

The exact Gameplay/read request identity is:

```text
BattleId + SelectionBoundaryRevision
```

Implementation rules:

- `FPendingSelectionRequestIdentity` is the one shared value type.
- `UDeferredSelectionAction` freezes the identity from the interactive-boundary `FPresentationRecordWriter` before `USelectionRequestAction` begins the pending request.
- Real Battle-owned Player selection queues reject a missing/invalid boundary identity.
- `USelectionResolver` stores and clears the identity with the pending request.
- `FPendingCardSelectionReadView` exposes the identity.
- Production submit/cancel APIs require the caller's expected identity and exact-match the resolver before interpreting RuntimeIds or applying cancellation.
- Production submit/cancel also verify that the expected BattleId still matches the BattleManager's latest frozen baseline, preventing cross-battle stale writes.
- The ViewModel only exposes a pending read surface when `RequestIdentity.BattleId == ViewModel.BattleId` and `SelectionBoundaryRevision == ViewModel.StateRevision`.
- The transient selected-card set stores its exact request identity and resets when the identity changes, even if source/count/candidates are identical.
- Existing `SelectionGeneration` remains a separate SelectionArea visual ownership identity and is not required for the first pending click.

Older generic Automation helpers keep test-only compatibility overloads for submit/cancel. A small isolated Selection primitive fixture with a non-Battle-owned Queue receives a deterministic test-only identity; real Battle-owned queues remain strict even in Editor builds.

### Readiness shadow evaluator

Added a shadow-only readiness model with explicit authority and modes:

```text
Authority:
- PresentationOwned
- DirectBaseline
- Invalid

Mode:
- NormalPlayerTurn
- CardReadyToConfirm
- CardTargetChoice
- PendingCardSelection
- Resolving
- TerminalOrUnavailable
```

The evaluator verifies:

- latest frozen `BattleId/StateRevision` equals the displayed ViewModel;
- player-facing read snapshot equals the displayed `BattleId/StateRevision`;
- PresentationOwned mode has a current exact `PresentationSessionToken`;
- DirectBaseline remains intentionally sessionless;
- authoritative pending selection has priority over the ordinary interaction enum;
- visible pending identity exactly matches the authoritative pending request;
- an active SelectionArea generation, when present, remains scoped to the same battle/boundary;
- target-choice EndTurn is false in the post-fix baseline.

The evaluator is **shadow-only** in G8-B. It never writes `bInputLocked`, never grants Gameplay input, and never advances a request.

### FastInput exact deferred request

`HasActiveNativePresentation()` keeps its original meaning: concrete tracked Native Widget playback.

G8-B adds Controller-authoritative:

```text
HasSkippablePresentationDelay()
```

In G8-B this is true only when active/backlogged authoritative Presentation chronology can be collapsed by the existing Skip path. Detached cosmetic state does not make it true. G8-C may later extend this query with compatibility debt.

A fast card click now freezes:

```text
PresentationSessionToken
+ BattleId
+ ExpectedCatchUpRevision
+ RuntimeId
```

`ExpectedCatchUpRevision` is the latest frozen baseline revision captured at the physical click, not the older displayed revision.

The next-tick retry proceeds only when:

```text
same current PresentationSessionToken
same BattleId
ViewModel.StateRevision == ExpectedCatchUpRevision
no authoritative PendingSelection exists
Outcome == None
InteractionState == Idle
input is unlocked
RuntimeId is still a displayed gameplay-playable Hand card
```

Any session replacement, battle/revision transition, pending-selection boundary, target/confirm/terminal surface, or missing card makes the old physical click stale and drops it.

A local/cosmetic visual without Controller-owned chronology no longer triggers FastInput Skip.

## Added/updated Automation coverage

New G8-B prefix:

```text
SlayTheSpireDemo.SelectionPresentation.G8B.TargetChoiceEndTurnBaseline
SlayTheSpireDemo.SelectionPresentation.G8B.TargetChoiceSwitchCardParity
SlayTheSpireDemo.SelectionPresentation.G8B.Readiness.DirectBaselineModes
SlayTheSpireDemo.SelectionPresentation.G8B.Readiness.PresentationAuthorityAndSkippableDelay
SlayTheSpireDemo.SelectionPresentation.G8B.PendingIdentity.ResolverABA
SlayTheSpireDemo.SelectionPresentation.G8B.PendingIdentity.ReadFenceAndStaleSubmit
SlayTheSpireDemo.SelectionPresentation.G8B.PendingIdentity.StaleCancel
SlayTheSpireDemo.SelectionPresentation.G8B.FastInput.StaleSessionDropsRetry
SlayTheSpireDemo.SelectionPresentation.G8B.FastInput.StaleRevisionDropsRetry
```

Updated affected FastInput regression:

```text
SlayTheSpireDemo.Phase6UIA2N.FastInput.DeferredRetry
SlayTheSpireDemo.Phase6UIA2N.FastInput.LocalVisualAloneDoesNotSkip
```

Existing affected regression that should remain green:

```text
SlayTheSpireDemo.CardSelection.Unified.ConsecutiveIdenticalSelectionsResetInput
SlayTheSpireDemo.CardSelection.Unified.NoHistoryKeepsPendingGameplaySelection
SlayTheSpireDemo.CardSelection.Unified.PresentationDegradationKeepsPendingSelection
```

The existing consecutive-identical-selection test already proves that partial transient UI state is cleared between two requests with the same source/candidate set. G8-B's new identity tests separately prove that those requests own different exact boundary identities and stale callbacks cannot ABA-match them.

## Validation required

G8-B is not validated until all of the following pass on the final `main` head:

```text
[ ] UE 5.8 project-file generation / Development Editor build
[ ] SlayTheSpireDemo.SelectionPresentation.G8B focused Automation
[ ] SlayTheSpireDemo.Phase6UIA2N.FastInput affected regression
[ ] affected Unified Selection regressions
```

No dedicated manual PIE visual gate is required for G8-B because the readiness evaluator is shadow-only and the prerequisite TargetChoice change is deterministic input routing/state. G8-C/D will own the visible detached-Damage overlap gates.

## Forward boundary

Do not begin G8-C from this checkpoint until G8-B Build and Automation are green.

Production state remains:

```text
Damage Presentation: Blocking
Detached DamageNumber: production disabled
Compatibility debt: not implemented
Readiness evaluator: shadow only
Earlier Gameplay unlock: not enabled
```
