# Card Expansion — Wave 1C-C0 Select-Exhaust Generalization

Date: **2026-09-08**

Status:

```text
DESIGN LOCKED / IMPLEMENTATION AUTHORIZED
DIRECT DEVELOPMENT ON main
```

Current branch policy for this slice:

```text
main
```

This document is the dedicated design authority for **Wave 1C-C0**.

Wave 1C-C0 is a capability-generalization slice that runs before the first Wave 1C-C production consumer. It does **not** author True Grit itself. Its purpose is to generalize the already-sealed `USelectExhaustHandCardEffect` from the Burning Pact-specific shape of "player selects exactly 1 Hand card" into a Blueprint-authored reusable effect that supports:

```text
- exactly N Hand cards
- Player selection mode
- Random selection mode
- explicit Base values
- explicit Upgraded values
```

The existing Wave 1A / 1B / 1C-A / 1C-B contracts remain sealed unless a concrete regression proves one of those contracts is wrong.

---

## 1. Why C0 exists

The sealed Wave 1C implementation already has a generic Selection request/result shape:

```text
FSelectionRequest
- Candidates[]
- MinCount
- MaxCount
- CancelPolicy

FSelectionResult
- SelectedObjects[]
```

However, the first Select-Exhaust consumer was intentionally narrow:

```text
USelectExhaustHandCardEffect
→ Player choice only
→ MinCount = 1
→ MaxCount = 1

UExhaustSelectedContinuation
→ requires exactly one selected object
→ builds one UExhaustCardAction

Native HUD bridge
→ supports one selected RuntimeId
→ immediately submits after one click
```

That shape was sufficient to seal Burning Pact, but it is not yet the reusable authored effect required by later cards.

C0 generalizes the consumer layer while preserving the generic Selection primitive and the existing exact-card Exhaust primitive.

---

## 2. Locked authored Effect contract

Keep the existing class identity:

```text
USelectExhaustHandCardEffect
```

Do **not** rename the UCLASS as part of C0. `DA_Card_BurningPact.uasset` is already repository content on `main`, and class identity / serialized defaults must remain compatible.

The Effect gains Blueprint-authored Base and Upgraded configuration:

```text
BaseSelectionMode
BaseSelectionCount

UpgradedSelectionMode
UpgradedSelectionCount
```

Selection mode is a typed Blueprint-visible enum with exactly these C0 values:

```text
Player
Random
```

Recommended naming may vary slightly during implementation, but the semantic contract above is locked.

### Explicit upgrade values

C0 follows the sealed ordinary-card upgrade model used by Draw / Block / Cost:

```text
Base value
+ explicit Upgraded value
+ UCardInstance::bUpgraded chooses the effective value
```

No sentinel/fallback semantics are allowed.

Forbidden examples:

```text
UpgradedSelectionCount = -1 means use Base
UpgradedSelectionMode = None means use Base
```

If an upgrade leaves a field unchanged, author the same explicit value in Base and Upgraded fields.

### Backward-compatible defaults

Default serialized C0 values must preserve current Burning Pact behavior:

```text
BaseSelectionMode      = Player
BaseSelectionCount     = 1
UpgradedSelectionMode  = Player
UpgradedSelectionCount = 1
```

Therefore an existing Burning Pact asset that has not yet been resaved must continue to behave as:

```text
Player / 1
→ Player / 1 when upgraded
```

---

## 3. Effective authored value rule

At `BuildActions` time:

```text
Context.Card->IsUpgraded() == false
→ use BaseSelectionMode / BaseSelectionCount

Context.Card->IsUpgraded() == true
→ use UpgradedSelectionMode / UpgradedSelectionCount
```

The Effect should expose narrow helpers equivalent to:

```text
GetEffectiveSelectionMode(bUpgraded)
GetEffectiveSelectionCount(bUpgraded)
```

The exact C++ spelling is not architecture authority; the typed Base/Upgraded semantics are.

---

## 4. Candidate rule — unchanged from sealed Wave 1C

C0 continues to select from the current Hand and excludes the played card:

```text
Candidates
= current Hand cards
- Context.Card
- invalid runtime objects
```

Do not add zone-generic selection in C0.

The Effect remains specifically a **Hand Select-Exhaust** composition point.

ExhaustPile selection, DiscardPile selection and generic any-zone movement belong to later consumers/slices.

---

## 5. Exactly-N semantics

C0 supports **exactly N** selected cards, not a user-selectable range.

Configured count:

```text
ConfiguredSelectionCount >= 0
```

Runtime effective count:

```text
RequiredCount = Min(ConfiguredSelectionCount, CandidateCount)
```

Rules:

```text
ConfiguredSelectionCount == 0
→ no-op
→ no pending selection
→ no RNG consumption
→ no Exhaust action
→ later authored Effects continue normally

CandidateCount == 0
→ no-op
→ no pending selection
→ no RNG consumption
→ no ResolutionFault
→ later authored Effects continue normally

ConfiguredSelectionCount > CandidateCount
→ RequiredCount = CandidateCount
→ select/exhaust every valid candidate
```

C0 does not treat "not enough cards" as a playability error.

If a future card must be unplayable unless at least N candidates exist, that belongs to the Card playability/rule surface, not to this Effect.

---

## 6. Player mode

When:

```text
EffectiveSelectionMode = Player
RequiredCount > 0
```

Gameplay authors one mandatory Selection request:

```text
MinCount = RequiredCount
MaxCount = RequiredCount
CancelPolicy = Forbidden
Candidates = exact ordered Hand candidates
```

This keeps Select-Exhaust as a mandatory authored cost/step.

### Native HUD exact-N interaction

The Native HUD must support an exact-N pending card-selection read view containing only UI-safe facts:

```text
SelectionSource
CandidateRuntimeIds[]
RequiredCount
CanCancel
```

The UI still receives no authoritative `UCardInstance*` candidate pointers.

Interaction contract:

```text
RequiredCount = 3

click candidate A
→ locally selected
→ 1 / 3

click candidate C
→ locally selected
→ 2 / 3

click already-selected A
→ locally deselected
→ 1 / 3

reach exactly 3 unique RuntimeIds
→ auto-submit one complete selection result
```

No Confirm button is added in C0.

Reason:

```text
C0 semantics = exactly N
```

A future `0..N`, `1..N`, or optional-selection consumer may authorize an explicit Confirm surface later.

### UI state is not Gameplay authority

The Native HUD/ViewModel may hold transient selected RuntimeIds for interaction/highlight purposes only.

It must not hold authoritative selected `UObject*` pointers and must not construct/enqueue BattleActions.

Transient selected RuntimeIds must be cleared when:

```text
- the selection resolves
- the selection disappears/is replaced
- the widget/view model resets
- cancellation succeeds on a future allowed request
- Presentation/battle teardown invalidates the interaction surface
```

---

## 7. Gameplay submission boundary for Player multi-select

The single-RuntimeId facade is generalized to accept an exact set of RuntimeIds:

```text
RuntimeIds[]
→ Gameplay facade
→ resolve against the current authoritative pending request
→ validate
→ construct FSelectionResult inside Gameplay
→ USelectionResolver::SubmitResult
```

Gameplay validation must reject:

```text
- wrong selected count
- INDEX_NONE / invalid RuntimeId
- duplicate RuntimeIds
- RuntimeId not present in current candidate set
- stale RuntimeId whose candidate object no longer matches
- invalid/non-card candidate on the card-selection facade
```

The facade should canonicalize the submitted selection to the request's stable candidate order before constructing `FSelectionResult`.

The UI click order must not become implicit Gameplay execution order.

---

## 8. Selection core duplicate-result hardening

Wave 1C `USelectionResolver` already validates:

```text
selected count within [MinCount, MaxCount]
selected object is valid
selected object belongs to candidate set
```

C0 adds a generic primitive correctness rule:

```text
SelectedObjects must be unique by authoritative object identity
```

Therefore a malformed result such as:

```text
[A, A]
```

must be rejected even if the requested count is 2.

This validation belongs in the generic Selection resolver. UI duplicate prevention is not sufficient authority.

---

## 9. Random mode

When:

```text
EffectiveSelectionMode = Random
RequiredCount > 0
```

C0 must **not** create a pending player-selection UI.

Flow:

```text
ordered candidates
→ deterministic battle RNG
→ choose RequiredCount unique candidates without replacement
→ canonicalize chosen set to original candidate order
→ FSelectionResult(Status=Resolved, SelectedObjects=[...])
→ same authored exhaust continuation
```

Random and Player modes differ only in **how a valid SelectionResult is produced**.

They must converge on the same downstream typed composition contract.

Forbidden:

```text
URandomExhaustCardAction
TrueGrit-specific random Action
random mode faking Native HUD clicks
random mode directly mutating DeckRuntime zones
```

---

## 10. Deterministic RNG ownership/API boundary

C0 is the first authorized real consumer requiring random card choice.

Current battle-local deterministic RNG state is owned by `UDeckRuntime::RandomStream` and is already used by deterministic shuffle.

C0 may expose the **smallest domain-neutral random-index helper** from the current battle RNG owner, equivalent in meaning to:

```text
TryChooseRandomIndex(Count, OutIndex)
```

or:

```text
ChooseIndex(Count)
```

The exact method spelling is implementation detail.

Locked semantics:

```text
input  = integer candidate count
output = deterministic integer index in [0, Count)
```

The RNG helper must not know:

```text
Card
Hand
Exhaust
Selection
True Grit
CardId
```

C0 does not move or duplicate the authoritative RNG stream into a new subsystem merely to satisfy this consumer.

### Random multi-select without replacement

For N selections:

```text
working candidates = ordered candidate list
repeat N times:
  choose deterministic index from remaining working candidates
  record that candidate
  remove it from the temporary working list
```

This guarantees unique chosen cards.

After the set has been chosen, reorder/canonicalize to the original candidate order before building downstream Exhaust actions.

This separates:

```text
RNG decides membership
candidate order decides deterministic execution order
```

---

## 11. Stable multi-exhaust execution order

A multi-card selection represents a set of cards, not an authored Trigger ordering chosen by click timing.

Example:

```text
candidate order: A B C D
player click order: C A B
```

Gameplay canonical result:

```text
A B C
```

Random mode follows the same rule after membership is chosen.

This is important because each Exhaust may later dispatch independent `CardExhausted` reactions. Player click order must not become a hidden way to control reaction order.

---

## 12. Continuation generalization

Keep the existing authored continuation identity unless implementation review proves a rename is required for correctness:

```text
UExhaustSelectedContinuation
```

C0 changes its semantic shape from:

```text
one selected card
→ one UExhaustCardAction
```

to:

```text
N selected cards in canonical candidate order
→ UExhaustCardAction(A)
→ UExhaustCardAction(B)
→ ...
```

Do **not** introduce a `BulkExhaustAction` in C0.

Each selected card must retain the sealed Wave 1B exact-card path:

```text
UExhaustCardAction
→ exact Hand card authoritative commit
→ exact FCardZoneMutationResult
→ committed CardZoneChanged
→ exact FCardExhaustedEvent
→ Dispatcher
```

This preserves one committed Exhaust fact per real card mutation.

---

## 13. Reaction ordering precedent

C0 does not add new reactive Powers, but multi-exhaust must remain compatible with future Wave 1D.

For selected cards A, B, C:

```text
ExhaustCardAction(A)
→ commit A
→ dispatch CardExhausted(A)
→ same-commit reactions (when future listeners exist)

then

ExhaustCardAction(B)
→ commit B
→ dispatch CardExhausted(B)
→ reactions

then C
```

C0 must not collapse these commits into one bulk fact.

Existing BattleActionQueue / Dispatcher ordering remains authoritative.

---

## 14. Presentation contract

C0 does not redesign the sealed Hand→Exhaust visual.

Each selected card continues to use:

```text
translation 0 → 0
scale       1 → 1
opacity     1 → 0
```

For N selected cards, committed Presentation is sequential in the same canonical execution order:

```text
Hand→Exhaust(A) fade/reduce
→ Hand→Exhaust(B) fade/reduce
→ Hand→Exhaust(C) fade/reduce
```

The formal Hand is reduced after each exact record before the next record is presented.

Required preservation:

```text
- no exhausted card reappears at Hand tail
- no movement-to-Exhaust animation returns
- later committed records keep their existing playback order
```

---

## 15. Description / upgrade text value

`USelectExhaustHandCardEffect` gains a Blueprint-authored integer description argument similar to Draw/Block, e.g.:

```text
DescriptionArgumentName = Exhaust
```

Effective preview value:

```text
Base card     → BaseSelectionCount
Upgraded card → UpgradedSelectionCount
```

This allows authored formats such as:

```text
"Exhaust {Exhaust} cards. Draw {Draw} cards."
```

C0 must validate that:

```text
DescriptionArgumentName is not None
BaseSelectionCount >= 0
UpgradedSelectionCount >= 0
```

Mode-specific text such as "random" vs "choose" may be exposed through a typed text argument if required by the C1 consumer, but C0 must not hard-code True Grit-specific card wording.

---

## 16. Burning Pact regression compatibility

C0 must preserve the already-sealed Burning Pact behavior without requiring a card-specific code path.

Effective Burning Pact configuration remains:

```text
Base
Player / 1

Upgrade
Player / 1
```

Expected resolution remains:

```text
play Burning Pact
→ mandatory one-card Player selection
→ chosen Hand card fades/exhausts
→ Draw 2 (or Draw 3 upgraded)
→ Burning Pact finishes to Discard
```

The existing Wave 1C 13-test suite remains a required regression gate.

---

## 17. Expected implementation surface

C0 is expected to touch only the smallest relevant layers.

Likely source areas:

```text
Cards/Effects/SelectExhaustHandCardEffect.*
Selection/SelectionResolver.*
Selection/ExhaustSelectedContinuation.*
Battle/BattleSelectionRequest.*
UI/BattleHUDViewModel.* / BattleHUDViewModelSelection.cpp
UI/BattleHUDWidget* pending-selection input path
Deck/DeckRuntime.* (narrow domain-neutral RNG index helper)
SlayTheSpireDemoTests/Private/*Wave1CC0* or focused Wave1C extension tests
```

This list is guidance, not permission to broaden scope.

Do not modify unrelated sealed systems merely because they are nearby.

---

## 18. Focused Automation requirements

C0 focused coverage must include at minimum:

```text
Authored configuration
- defaults preserve Player / 1
- Base vs Upgraded mode selection
- Base vs Upgraded count selection
- count 0 is no-op
- count greater than candidates clamps to all candidates

Generic Selection hardening
- duplicate selected object rejected
- wrong count rejected
- foreign object rejected

Player multi-select Gameplay facade
- read view exposes RequiredCount + candidate RuntimeIds only
- exact N unique RuntimeIds accepted
- duplicate RuntimeIds rejected
- stale/foreign RuntimeId rejected
- canonical result order follows candidate order, not submission order

Continuation
- N selected cards build N UExhaustCardActions
- each exact card exhausts once
- execution order follows canonical candidate order

Random
- deterministic single selection
- deterministic multi selection
- multi selection has no duplicates
- same seed/state produces same membership
- Random mode creates no pending Native selection
- Random mode consumes the same authored continuation path

Presentation
- sequential Hand→Exhaust records reduce exact cards correctly
- multi-card order remains stable

Regression
- existing Wave 1C 13/13 remains PASS
- Burning Pact Player/1 behavior remains unchanged
```

Automation must not depend on a newly authored True Grit binary asset.

Use transient C++ fixtures for C0 capability testing.

---

## 19. PIE acceptance requirements

Before C0 can be sealed, use temporary/test content to validate at least:

### Player / 3

```text
play test card
→ Native HUD enters pending selection
→ choose/deselect candidates
→ reaching exactly 3 auto-submits
→ the 3 canonical selected cards fade/exhaust sequentially
→ Hand remains correct
→ no ghost/tail reappearance
→ normal input returns
```

### Random / 3

```text
play test card
→ no pending player-selection UI
→ 3 unique valid candidates are selected deterministically
→ those cards fade/exhaust sequentially in canonical order
→ normal input returns
```

### Burning Pact regression

```text
Player / 1 remains unchanged
Draw 2 / Draw 3 playback remains correct
```

---

## 20. Seal gates

Wave 1C-C0 becomes `COMPLETE / VALIDATED / SEALED` only after all of the following pass on the implementation head:

```text
[ ] SlayTheSpireDemoEditor Win64 Development Build PASS
[ ] C0 focused Automation PASS
[ ] existing SlayTheSpireDemo.CardExpansion.Wave1C 13/13 regression PASS
[ ] Native HUD PIE Player multi-select PASS
[ ] Native HUD PIE Random multi-select PASS
[ ] Native HUD PIE Burning Pact regression PASS
[ ] user validation/seal confirmation
```

Until then the implementation status is:

```text
DESIGN LOCKED / IMPLEMENTATION ACTIVE / NOT SEALED
```

---

## 21. Explicit non-goals

Wave 1C-C0 does **not** implement or authorize:

```text
- production True Grit CardData
- Exhume
- ExhaustPile selection UI
- DiscardPile selection UI
- generic any-zone selection/move
- optional 0..N / 1..N selection ranges
- Confirm button UI
- BulkExhaustAction
- random enemy selection
- multi-enemy combat
- Feel No Pain
- Dark Embrace
- Card Trigger Source Expansion / Sentinel
- generic Card rule pipeline changes
- Phase 8
```

C0 also does not reopen sealed CFV, ordinary Upgrade, Wave 1A, Wave 1B, or the existing committed-Presentation ownership model.

---

## 22. C0 completion handoff

After C0 is sealed, Wave 1C-C1 may author True Grit primarily as content composition:

```text
GainBlockCardEffect
+
USelectExhaustHandCardEffect
  Base     = Random / 1
  Upgraded = Player / 1
```

If True Grit C1 still requires a card-specific Gameplay Action for its core selection/exhaust behavior, C0 should be considered incomplete unless a concrete card rule proves the generic contract above insufficient.

The intended handoff is:

```text
C0
→ generalize the reusable capability
→ seal it

C1 True Grit
→ consume the capability as authored card data/composition
```
