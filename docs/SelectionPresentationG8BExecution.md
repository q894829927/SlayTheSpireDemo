# Selection Presentation G8-B Execution

Date: 2026-09-11

Status: **COMPLETE / VALIDATED ON MAIN**.

G8-B implements the readiness, exact PendingSelection identity, and FastInput shadow contracts defined by `docs/SelectionPresentationG8Design.md`. It does not enable G8-C detached Damage production, compatibility debt, or earlier Gameplay unlock.

Implementation base:

```text
a52f2c8537c720589be1669963e01a4137127f33
G8-A validated checkpoint
```

Original G8-B runtime implementation checkpoint:

```text
5bb6998ce7e88992557b1bcfc9cea7b42e5783f4
```

Final validated `main` checkpoint after validation fixes:

```text
6add7e0c48ffa53ffc6f64c1e746910688a1494e
```

## Implemented scope

### Prerequisite baseline fixes

- `ChoosingTarget` explicitly disables `bCanEndTurn`.
- `RequestEndTurn()` rejects while `ChoosingTarget` without clearing the selected card/target-choice surface.
- Selecting another legal Hand card while `ChoosingTarget` remains allowed and rebuilds the new card's target surface.
- Controller initialization failure remains the G8-A `PresentationUnavailable` fail-safe; G8-B does not restore DirectBaseline fallback.

### Exact PendingSelection identity

The exact Gameplay/read request identity is:

```text
BattleId + SelectionBoundaryRevision
```

Implementation rules:

- `FPendingSelectionRequestIdentity` is the shared value type.
- `UDeferredSelectionAction` freezes the identity from the interactive-boundary `FPresentationRecordWriter` before `USelectionRequestAction` begins the pending request.
- Real Battle-owned Player selection queues reject a missing/invalid boundary identity.
- `USelectionResolver` stores and clears the identity with the pending request.
- `FPendingCardSelectionReadView` exposes the identity.
- Production submit/cancel APIs require the caller's expected identity and exact-match the resolver before interpreting RuntimeIds or applying cancellation.
- Production submit/cancel also verify that expected `BattleId` matches the BattleManager's latest frozen baseline, preventing cross-battle stale writes.
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
- ordinary player-facing surfaces require the player-facing read snapshot to match the displayed `BattleId/StateRevision`;
- PendingSelection may use the sealed identity-only coherent read edge already defined by `TryPublishReadStateReady()` when ordinary player-facing read construction is intentionally unavailable while Gameplay waits on the player decision;
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

A fast card click freezes:

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

A local/cosmetic visual without Controller-owned chronology does not trigger FastInput Skip.

## Validation fixes found by Automation

The first G8-B Automation run exposed two fixture issues and one real readiness-shadow defect:

1. `FastInput.StaleSessionDropsRetry` and `FastInput.StaleRevisionDropsRetry` initially started synthetic Controller playback while the ViewModel was still `Idle + unlocked`, so the click correctly took the ordinary selection path instead of the deferred FastInput path. The fixtures now recreate the required `Resolving + input locked` catch-up surface before playback starts.
2. `PendingIdentity.ReadFenceAndStaleSubmit` exposed that the readiness shadow incorrectly required an ordinary player-facing read during a PendingSelection identity-only public edge. The evaluator now accepts the exact frozen pending boundary defined by the BattleManager contract while keeping ordinary surfaces strict.
3. The affected `Phase6UIA2N.FastInput` regression fixtures were aligned with the same semantics: Controller-owned deferred retry is tested from a real locked catch-up surface; a local-only tracked visual is not treated as authoritative skippable chronology, while existing ViewModel dirty-change cancellation remains allowed.

No Gameplay authority or sealed G0-G7 contract was relaxed by these fixes.

## Validation evidence

Final validation reported from UE 5.8 on the validated `main` checkpoint:

```text
UE 5.8 Development Editor build                         PASS
SlayTheSpireDemo.SelectionPresentation.G8B             9/9 PASS
SlayTheSpireDemo.Phase6UIA2N.FastInput                 2/2 PASS
SlayTheSpireDemo.CardSelection.Unified                 14/14 PASS
Unified warnings                                       2 expected, 0 errors
```

The two Unified warnings are intentional failure/degradation-path logs:

```text
[Selection] BeginSelection rejected: a selection is already pending.
[Presentation] Unavailable ... Could not freeze the exact interactive pre-selection Presentation snapshot.
```

The required G8-B affected regressions are therefore green, including:

```text
SlayTheSpireDemo.CardSelection.Unified.ConsecutiveIdenticalSelectionsResetInput
SlayTheSpireDemo.CardSelection.Unified.NoHistoryKeepsPendingGameplaySelection
SlayTheSpireDemo.CardSelection.Unified.PresentationDegradationKeepsPendingSelection
```

No dedicated manual PIE visual gate is required for G8-B because the readiness evaluator is shadow-only and the prerequisite TargetChoice change is deterministic input routing/state. G8-C/D own the visible detached-Damage overlap gates.

## Final G8-B state

```text
Build:                         PASS
Focused Automation:            PASS (9/9)
FastInput regression:          PASS (2/2)
Unified Selection regression:  PASS (14/14; 2 expected warnings)
G8-B:                          COMPLETE / VALIDATED
```

## Forward boundary

G8-C may proceed from this validated G8-B baseline.

Production state at the end of G8-B remains:

```text
Damage Presentation: Blocking
Detached DamageNumber: production disabled
Compatibility debt: not implemented
Readiness evaluator: shadow only
Earlier Gameplay unlock: not enabled
```
