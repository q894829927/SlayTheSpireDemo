# Card Expansion — Wave 1C-C1 Warcry / Hand→DrawPileTop Plan

Date: **2026-09-08**

Status:

```text
DESIGN PROPOSED / BRANCH ACTIVE
NO PRODUCTION IMPLEMENTATION YET
C0 VALIDATION DEFERRED BY USER
FINAL MERGE / SEAL BLOCKED UNTIL C0 SEAL
```

Branch:

```text
Wave-1C-C1
```

Branch base:

```text
main @ c700db13a50d4c3c6302e836424de3e4693fafa4
```

This plan supersedes the abandoned True Grit consumer plan on this branch. True Grit remains implementable from existing C0 capabilities and is no longer the active C1 development target.

---

## 1. Goal

Add the smallest reusable capability needed for a Warcry-style card:

```text
Draw card(s)
→ choose one card from the CURRENT Hand
→ move that exact CardInstance from Hand to the TOP of DrawPile
→ later card-play cleanup handles the played card normally
```

The first intended consumer is Warcry, but the new Gameplay primitive must not know `Warcry` or any CardId.

Primary capability opened by this slice:

```text
CAP-05 — exact card zone mutation / draw-pile top insertion
```

Existing capabilities reused:

```text
CAP-04 Selection
CAP-06 authored Continuation
existing DrawCardEffect / DrawCardsAction
existing CardZoneChanged committed Presentation
existing Native HUD card-selection UI
```

---

## 2. Critical ordering constraint — selection must be execution-time

Current `UPlayCardAction` calls every authored Effect's `BuildActions(...)` before the follow-up Action batch begins executing.

Therefore this is incorrect for Warcry:

```text
BuildActions time
→ snapshot current Hand candidates
→ enqueue Draw
→ enqueue SelectionRequest using the old candidate snapshot
```

because the card(s) drawn by the previous Effect would not be selectable.

C1 must preserve authored Effect order while delaying candidate enumeration until the selection step actually executes:

```text
PlayCardAction
→ Effects build follow-up Actions

Queue executes:
DrawCardsAction
→ all dependent Draw / Shuffle work finishes
→ DeferredHandSelectionAction executes
→ reads CURRENT Hand
→ creates ordinary USelectionRequestAction
→ existing Selection resolver / HUD lifecycle
```

This deferred step is a neutral Hand-selection orchestration Action. It does not know DrawPileTop or Warcry.

---

## 3. DeckRuntime primitive

Add exactly one new authoritative mutation:

```cpp
FCardZoneMutationResult TryMoveHandCardToDrawPileTopCommit(UCardInstance* Card);
```

Contract:

```text
preconditions
- Card is valid
- exact CardInstance currently exists in Hand

commit
FromZone  = Hand
ToZone    = DrawPile
FromIndex = exact current Hand index
ToIndex   = DrawPile.Num() before insertion

mutation
Hand.RemoveAt(FromIndex)
DrawPile.Add(Card)
```

The existing deck convention is locked:

```text
DrawPile array order = bottom -> top
DrawPile.Last()      = next card drawn
```

Therefore `Add(Card)` places the exact card on top.

No new Result type is authorized. Existing `FCardZoneMutationResult` already carries all required committed facts.

No RNG or shuffle is consumed by this mutation.

Failure:

```text
invalid Card / Card not in Hand
→ bCommitted = false
→ no partial mutation
```

---

## 4. Exact move Action

Add:

```text
UMoveHandCardToDrawPileTopAction
```

Responsibilities:

```text
- hold exact Deck + CardInstance
- verify the exact card is still in Hand
- call TryMoveHandCardToDrawPileTopCommit
- validate committed CardRuntimeId/CardId/zones
- write one committed CardZoneChanged Presentation record
- expose its FCardZoneMutationResult for focused tests
```

It must NOT:

```text
- know Selection
- know Warcry
- know DrawCardEffect
- dispatch CardExhausted
- shuffle
- choose cards
- inspect CardId
```

Presentation card snapshot should use the existing frozen-card snapshot builder and the normal presentation writer inherited from `UBattleAction`.

Failure semantics should mirror the existing exact targeted Exhaust Action where applicable:

```text
invalid runtime dependency / exact card no longer in Hand / commit rejected
→ no mutation, finish fail-soft

committed facts inconsistent with the requested exact move
→ ResolutionFault
```

---

## 5. Selection continuation

Add a stateless authored continuation:

```text
UMoveSelectedHandCardToDrawPileTopContinuation
```

Contract for C1:

```text
Resolved result
SelectedObjects.Num() == 1
selected object must be a valid UCardInstance
→ build exactly one UMoveHandCardToDrawPileTopAction
```

The continuation does not mutate Gameplay or drive the queue directly.

It remains resolution-local and follows the existing `UAuthoredContinuation` contract.

C1 does not authorize multi-card top insertion.

---

## 6. Deferred current-Hand selection Action

Add a narrow neutral orchestration Action, proposed name:

```text
UDeferredHandSelectionAction
```

C1-authorized semantics:

```text
Execute-time only
→ inspect UDeckRuntime::GetHandCards()
→ build candidates from valid current Hand CardInstances
→ exact one required
→ CancelPolicy = Forbidden
→ if no candidates: legal no-op, Finish
→ otherwise create ordinary USelectionRequestAction
→ propagate current PresentationRecordWriter
→ insert that SelectionRequestAction at Queue front
→ Finish
```

Inputs:

```text
UDeckRuntime*
USelectionResolver*
UAuthoredContinuation*
SelectionSource
```

This Action may know that candidates come from Hand. It must not know how the selected card will later be used.

Do not modify `USelectionRequestAction` into a candidate-provider framework in C1. Do not add arbitrary callbacks, predicates, zone providers or generic query buses.

Do not refactor existing C0 SelectExhaust to use this Action during C1 unless a concrete correctness issue requires it.

---

## 7. Authored Effect

Add the new reusable card Effect:

```text
USelectHandCardToDrawPileTopEffect
```

Its `BuildActions(...)` does NOT enumerate the Hand.

It builds:

```text
UMoveSelectedHandCardToDrawPileTopContinuation
+
UDeferredHandSelectionAction
```

The deferred Action later produces the existing `USelectionRequestAction` from the current Hand.

C1 Effect semantics are fixed:

```text
selection mode  = Player
selection count = exactly 1
cancel          = Forbidden
candidate zone  = current Hand at execution-time
no candidates   = no-op
```

No Blueprint count/range/random configuration is authorized for this Effect in C1 because neither Warcry nor the next obvious reuse case requires it.

If a second real consumer later requires different count or selection mode, generalize then.

---

## 8. Presentation reducer contract

`FPresentationStateSnapshot` intentionally does not expose the hidden DrawPile card identities; it exposes only `DrawCount`.

For committed:

```text
Hand -> DrawPile
```

the reducer should validate:

```text
- payload card RuntimeId is present at the claimed/current Hand index
- ToIndex == WorkingPresentationSnapshot.DrawCount
```

then apply:

```text
HandCards.RemoveAt(FromIndex)
DrawCount += 1
```

Do not add a public DrawPile card array to Presentation state merely for this feature.

The authoritative top-card identity remains in `UDeckRuntime`.

A subsequent Draw commit proves that the exact moved RuntimeId is the next top card.

---

## 9. Native visual contract

Destination DrawPile is hidden, so C1 does not need a fake DrawPile card widget or a new public pile-identity model.

Minimum committed playback:

```text
selected Hand card
→ remains in its current slot during transition
→ short in-place removal fade
→ no translation requirement
→ scale remains 1
→ opacity 1 -> 0
→ formal Hand removes the exact RuntimeId
→ DrawCount increments
```

The implementation may reuse an existing exact-Hand-card fade helper if compatible.

Required:

```text
- no ghost/tail reappearance
- no accidental Exhaust counter change
- no fake Draw animation
- no UI mutation before committed CardZoneChanged playback
- pending selection highlight clears on submit as already sealed by C0
```

Presentation style is intentionally narrow; no general zone-animation framework is authorized.

---

## 10. Warcry transient consumer proof

After the primitive Effect is implemented, prove it with a transient C++ Warcry definition before production asset work.

Intended composition:

```text
DrawCardEffect
+
USelectHandCardToDrawPileTopEffect
+
DefaultDestination = Exhaust
```

Expected authored values for the later production consumer:

```text
CardId             = Warcry
CardType           = Skill
Rarity             = Common
CardColor          = Red
BaseCost           = 0
UpgradedCost       = 0
TargetType         = None
DefaultDestination = Exhaust

DrawCardEffect
Base     = 1
Upgraded = 2
```

Execution must demonstrate the ordering problem is solved:

```text
play Warcry
→ Warcry enters PlayArea
→ Draw 1 / 2 fully resolves
→ deferred Hand selection enumerates the POST-DRAW Hand
→ newly drawn card is eligible for selection
→ choose exact card
→ Hand -> DrawPileTop commit
→ Warcry finishes PlayArea -> Exhaust
```

If Draw requires Shuffle, all Draw/Shuffle continuation work must complete before the deferred selection Action runs.

---

## 11. Focused Automation plan

Proposed filter:

```text
SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop
```

Minimum coverage:

### Deck primitive

```text
Move.HandToDrawTopCommit
- exact Hand card moves
- FromIndex exact
- ToIndex equals pre-move DrawCount
- moved RuntimeId becomes DrawPile.Last()
- immediate TryDrawTopCardCommit returns the same exact CardInstance
- null / foreign / non-Hand card rejects without mutation
```

### Move Action

```text
Move.ActionCommitFacts
- one exact move
- exact typed commit result
- one CardZoneChanged record when writer available
- no CardExhausted event
```

### Deferred selection timing

```text
Selection.UsesPostPriorActionHand
- queue prior action adds/draws a card into Hand
- deferred selection runs afterward
- pending candidate RuntimeIds include the newly arrived card
```

This is the critical regression test for Warcry correctness.

### No candidates

```text
Selection.NoCandidatesNoOp
- no pending selection
- later queue work continues
- no move record
```

### Effect

```text
Effect.ExactOneMandatory
- current-Hand exact one request
- cancel forbidden
- selected exact card moves to DrawPile top
```

### Presentation

```text
Presentation.HandToDrawTopReducer
- exact Hand RuntimeId removed
- DrawCount +1
- stale FromIndex rejected
- wrong ToIndex rejected
```

### Transient Warcry

```text
Warcry.BaseExecution
- Draw 1 first
- post-draw Hand selection
- selected exact card becomes top
- Warcry self-exhausts

Warcry.UpgradedExecution
- Draw 2 first
- post-draw Hand selection
- selected exact card becomes top

Warcry.DrawShuffleThenSelection
- empty DrawPile + non-empty Discard triggers existing shuffle path
- requested Draw completes
- only then selection appears
```

---

## 12. Expected implementation surface

Authorized source areas:

```text
Deck/DeckRuntime.*
Actions/MoveHandCardToDrawPileTopAction.*
Actions/DeferredHandSelectionAction.*
Selection/MoveSelectedHandCardToDrawPileTopContinuation.*
Cards/Effects/SelectHandCardToDrawPileTopEffect.*
Presentation/BattlePresentationController.cpp
UI/BattleHUDWidget* only for committed Hand->DrawPile playback if required
SlayTheSpireDemoTests/Private/*Wave1CC1*
```

Existing `DeckMutationTypes.h` should not need a new result struct or zone enum.

---

## 13. Explicit non-goals

C1 does NOT authorize:

```text
- arbitrary-zone generic MoveCardAction
- generic FromZone / ToZone enum-driven mutation API
- DiscardPile selection
- Headbutt production behavior
- ExhaustPile selection
- Exhume
- multi-card top insertion
- optional 0..N / 1..N selection ranges
- random selection for this Effect
- arbitrary insertion index
- DrawPile card identity exposure to UI
- card creation/copy
- Havoc auto-play
- production Warcry .uasset before explicit asset-authoring step
- True Grit work
- reactive Powers
- multi-enemy work
```

---

## 14. Implementation slices

Recommended order:

```text
C1-1  DeckRuntime exact Hand -> DrawPileTop commit
C1-2  UMoveHandCardToDrawPileTopAction + committed Presentation record
C1-3  UMoveSelectedHandCardToDrawPileTopContinuation
C1-4  UDeferredHandSelectionAction (execution-time candidates)
C1-5  USelectHandCardToDrawPileTopEffect
C1-6  Presentation reducer / Native playback
C1-7  focused Automation
C1-8  transient Warcry composition proof
C1-9  production Warcry asset / PIE only after explicit authorization
```

Do not implement C1-9 as part of the Effect slice by default.

---

## 15. Validation / seal gates

C1 capability implementation is not sealed until:

```text
[ ] SlayTheSpireDemoEditor Win64 Development Build PASS
[ ] Wave1CC1.DrawPileTop focused Automation PASS
[ ] exact next-draw identity regression PASS
[ ] deferred post-Draw candidate regression PASS
[ ] Native HUD Hand->DrawPileTop playback PIE PASS
[ ] transient Warcry Base PIE PASS
[ ] transient Warcry Upgraded PIE PASS
[ ] existing C0 / Wave1C regressions remain green after deferred C0 validation is completed
[ ] user validation / seal confirmation
```

Until then:

```text
DESIGN PROPOSED / IMPLEMENTATION NOT STARTED / NOT SEALED
```
