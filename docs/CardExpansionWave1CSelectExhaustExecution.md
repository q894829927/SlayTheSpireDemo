# Card Expansion — Wave 1C-B Select-Exhaust / Burning Pact Closure Record

Date: **2026-09-07**

Status:

```text
IMPLEMENTATION UPDATED / LOCAL REVALIDATION REQUIRED
```

Branch:

```text
Wave-1C
```

Authority:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
```

This record supersedes the earlier 2026-09-06 `7/7 PASS / READY FOR SEAL` status for the current branch head. The previous result remains historical evidence for the pre-closure implementation, but the 2026-09-07 UI/cancel/E2E changes require a fresh build, focused Automation run and Native HUD PIE acceptance before sealing.

Repository-local `DA_Card_BurningPact.uasset` and `L_BattleTest.umap` changes are owner-authored ad-hoc test content. They are intentionally not modified, inspected, or counted as production validation evidence by this closure change.

---

## Closure problems addressed

### 1. Playable Native HUD selection bridge

Before this change, `USelectionRequestAction` correctly held the ActionQueue while waiting, but the Native HUD normal card-input path remained locked in `Resolving` and had no request route into the pending selection.

Implemented:

```text
Source/SlayTheSpireDemo/Battle/BattleSelectionRequest.h/.cpp
Source/SlayTheSpireDemo/UI/BattleHUDViewModelSelection.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidgetFastInput.cpp
```

Current route:

```text
existing Native Hand card click
→ UBattleHUDWidget::SelectCard(RuntimeId)
→ detect actual pending single-card Gameplay selection
→ UBattleHUDViewModel::SubmitPendingCardSelectionByRuntimeId
→ BattleSelectionRequest::SubmitPendingCardSelection
→ resolve RuntimeId against authoritative pending candidates
→ USelectionResolver::SubmitResult
→ awaiting USelectionRequestAction resumes
```

Boundary properties:

```text
- normal Resolving input remains locked
- the alternate click path exists only while a supported pending card selection exists
- ViewModel/HUD receives RuntimeIds only, not candidate UObject pointers
- non-candidate RuntimeIds are rejected
- UI never constructs/enqueues BattleActions
- historical committed Presentation ownership is unchanged
```

### 2. Request-level mandatory cancel policy

Implemented:

```text
ESelectionCancelPolicy
FSelectionRequest::CancelPolicy
USelectionResolver::CanCancelPendingSelection()
policy-aware SubmitCancel / CancelSelection
USelectionRequestAction mandatory-cancel guard
```

Generic selection defaults to:

```text
CancelPolicy = Allowed
```

`USelectExhaustHandCardEffect` authors:

```text
CancelPolicy = Forbidden
```

For a mandatory request, cancel attempts:

```text
return false
keep pending request
keep SelectionRequestAction unfinished/current
keep ActionQueue busy
perform no mutation
emit no ResolutionFault
```

This closes the previous Burning Pact loophole where cancel could have allowed the already-authored Draw Action to continue without paying the exhaust requirement.

### 3. Full transient Burning Pact Automation shape

The focused 1C-B test source now authors a transient Burning Pact-shaped `UCardData` instead of relying on a binary production asset:

```text
CardType = Skill
BaseCost = 1
UpgradedCost = 1
DefaultDestination = Discard
Effects = [
  USelectExhaustHandCardEffect,
  UDrawCardEffect(DrawCount=2, UpgradedDrawCount=3)
]
```

This keeps binary asset authoring independent while testing the actual Gameplay composition contract.

---

## Updated focused Automation inventory

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C
```

Existing primitive cases (5):

```text
Selection.ResolveBuildsContinuation
Selection.InvalidSelectionRejected
Selection.CancelIsLegalPath
Selection.MalformedRequestRejected
Selection.CountBoundsEnforced
```

Updated SelectExhaust / Burning Pact cases (6):

```text
SelectExhaust.ExhaustsChosenCard
SelectExhaust.SkipsWhenNoCandidate
SelectExhaust.MandatoryCancelRejected
BurningPact.BaseExhaustThenDraw2
BurningPact.UpgradedExhaustThenDraw3
BurningPact.NativeHUDPendingCardClick
```

Total expected focused inventory after this change:

```text
11 cases
```

New coverage includes:

```text
mandatory request advertises Forbidden cancellation
SubmitCancel cannot release the waiting action
Cancelled SubmitResult cannot release the waiting action
valid selection still completes normally after rejected cancel
Burning Pact Effects order = SelectExhaust then Draw
base: exact selected card exhausts before Draw-2
upgrade: exact selected card exhausts before Draw-3
played card finishes to Discard
PlayArea is empty after FinishCardPlay
ActionQueue returns idle without ResolutionFault
Native HUD C++ SelectCard(RuntimeId) accepts a valid pending candidate while the
ordinary card-play path is otherwise in a busy resolution
non-candidate HUD click cannot release the pending request
```

### Validation status

These tests were authored/updated in GitHub during this closure change, but this environment does not execute the Unreal Editor build or Automation runner. Therefore no new PASS claim is recorded here.

Required rerun:

```text
[ ] SlayTheSpireDemoEditor Win64 Development Build
[ ] SlayTheSpireDemo.CardExpansion.Wave1C focused Automation (expected 11 cases)
[ ] Native HUD production PIE: play test Burning Pact, click another Hand card,
    observe selected card exhaust -> Draw 2/3 -> normal resolution completion
```

The old `7/7 PASS` result applies only to the pre-2026-09-07 implementation and must not be used to seal this new branch head.

---

## Source surface changed by the closure

```text
Source/SlayTheSpireDemo/Selection/SelectionTypes.h
Source/SlayTheSpireDemo/Selection/SelectionResolver.h/.cpp
Source/SlayTheSpireDemo/Actions/SelectionRequestAction.cpp
Source/SlayTheSpireDemo/Cards/Effects/SelectExhaustHandCardEffect.cpp
Source/SlayTheSpireDemo/Battle/BattleSelectionRequest.h/.cpp
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.h
Source/SlayTheSpireDemo/UI/BattleHUDViewModelSelection.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidgetFastInput.cpp
Source/SlayTheSpireDemoTests/Private/CardExpansionWave1CSelectExhaustTests.cpp
```

---

## Scope retained

Still outside this code slice:

```text
- no Random selection mode
- no generic multi-select Native UI
- no Exhume / True Grit consumer expansion
- no bulk exhaust
- no reactive exhaust powers
- no acceptance or modification of owner-authored DA_Card_BurningPact.uasset
- no acceptance or modification of owner-authored L_BattleTest.umap changes
```

Production card asset acceptance remains a separate user-owned step. The C++ transient definition is the authoritative Automation fixture for the Burning Pact behavior contract until that content step is explicitly validated.
