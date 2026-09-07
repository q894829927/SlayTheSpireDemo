# Card Expansion — Wave 1C-B Select-Exhaust / Burning Pact Closure Record

Date: **2026-09-07**

Status:

```text
COMPLETE / VALIDATED / SEALED
```

Branch:

```text
Wave-1C
```

Authority:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
```

This record supersedes the earlier pre-closure `7/7 PASS / READY FOR SEAL` state. The 2026-09-07 playable-selection, mandatory-cancel, committed-Presentation and Native HUD fixes have now been revalidated on the current Wave-1C branch by the user.

Repository-local `DA_Card_BurningPact.uasset` and `L_BattleTest.umap` remain owner-authored test content. They were not modified by the C++ closure fixes and are not used as the Automation authority for the reusable behavior contract.

---

## Sealed closure

### 1. Playable Native HUD selection bridge

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
- alternate click path exists only while a supported pending card selection exists
- ViewModel/HUD receives RuntimeIds only, not candidate UObject pointers
- non-candidate RuntimeIds are rejected
- UI never constructs/enqueues BattleActions
- historical committed Presentation ownership remains Gameplay/Controller driven
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

This prevents Burning Pact from skipping its exhaust requirement and continuing into Draw.

### 3. Selection continuation Presentation writer propagation

Selection-generated dependent Actions inherit the current committed-Presentation writer at the common `USelectionRequestAction` continuation boundary.

Therefore:

```text
selection resolves
→ UExhaustCardAction is created
→ same resolution Presentation writer is inherited
→ Hand → ExhaustPile CardZoneChanged record is committed
→ later Draw records remain ordered after the exhaust record
```

### 4. Hand → Exhaust Presentation reducer and Native HUD playback

`UBattlePresentationController` now reduces exact `Hand → ExhaustPile` records using RuntimeId/CardId/index validation and updates the historical working Hand before later Draw records are played.

The Native HUD presents a selected Hand card exhaust as an in-place fade:

```text
selected formal Hand card
→ translation stays 0
→ scale stays 1
→ opacity 1 → 0 over the Native presentation duration
→ record completes
→ Controller formally removes the exact card from Hand
→ Draw presentation continues
```

There is no Hand-to-Exhaust movement animation. Cancel/destruct cleanup restores the historical card transform/opacity when necessary.

This fixes the previous visible failure where the exhausted card could remain in the displayed Hand until FinalSnapshot correction and cause subsequent Draw records to miss their expected Hand index.

### 5. Full transient Burning Pact Automation shape

The focused test source authors a transient Burning Pact-shaped `UCardData` rather than depending on a binary card asset:

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

---

## Final focused Automation inventory

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C
```

Selection primitive cases (5):

```text
Selection.ResolveBuildsContinuation
Selection.InvalidSelectionRejected
Selection.CancelIsLegalPath
Selection.MalformedRequestRejected
Selection.CountBoundsEnforced
```

SelectExhaust / Burning Pact Gameplay + input cases (6):

```text
SelectExhaust.ExhaustsChosenCard
SelectExhaust.SkipsWhenNoCandidate
SelectExhaust.MandatoryCancelRejected
BurningPact.BaseExhaustThenDraw2
BurningPact.UpgradedExhaustThenDraw3
BurningPact.NativeHUDPendingCardClick
```

Committed-Presentation regression cases (2):

```text
BurningPact.PresentationRecordOrder
Presentation.HandExhaustReducer
```

Final focused inventory:

```text
5 + 6 + 2 = 13 cases
```

Coverage includes:

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
Native HUD C++ SelectCard(RuntimeId) accepts a valid pending candidate during the waiting resolution
non-candidate HUD click cannot release the pending request
committed Presentation order is CardPlayed → Hand→Exhaust → Draw → Draw → Finish
Controller reducer accepts exact Hand→Exhaust and rejects stale destination index
```

---

## Final validation evidence

User-confirmed local validation on **2026-09-07**:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.CardExpansion.Wave1C focused Automation PASS (13/13)
[x] Native production HUD PIE base Burning Pact PASS
[x] Native production HUD PIE Burning Pact+ PASS
[x] mandatory selection cannot be skipped/cancelled into Draw PASS
[x] exhausted card fades in place and disappears before Draw playback PASS
[x] Draw 2 / Draw 3 presentation playback PASS
[x] exhausted card no longer reappears at the end of Hand PASS
[x] normal HUD interaction returns after resolution PASS
```

The previous `7/7 PASS` result is retained only as historical evidence. The seal is based on the current branch revalidation above.

---

## Sealed source surface

```text
Source/SlayTheSpireDemo/Selection/SelectionTypes.h
Source/SlayTheSpireDemo/Selection/SelectionResolver.h/.cpp
Source/SlayTheSpireDemo/Actions/SelectionRequestAction.cpp
Source/SlayTheSpireDemo/Selection/ExhaustSelectedContinuation.h/.cpp
Source/SlayTheSpireDemo/Cards/Effects/SelectExhaustHandCardEffect.h/.cpp
Source/SlayTheSpireDemo/Battle/BattleSelectionRequest.h/.cpp
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.h
Source/SlayTheSpireDemo/UI/BattleHUDViewModelSelection.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h/.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidgetFastInput.cpp
Source/SlayTheSpireDemoTests/Private/CardExpansionWave1CSelectExhaustTests.cpp
Source/SlayTheSpireDemoTests/Private/CardExpansionWave1CPresentationTests.cpp
```

---

## Scope retained after seal

Still outside Wave 1C:

```text
- no Random selection mode
- no generic multi-select Native UI
- no Exhume / True Grit consumer expansion
- no bulk exhaust
- no reactive exhaust powers
- no generic AnyZone movement abstraction
```

The reusable C++/Gameplay/Presentation contract is sealed. Repository-local owner-authored binary test content remains a separate content-acceptance concern and is not what establishes the Wave 1C behavioral seal.