# Card Expansion — Wave 1C Selection Primitive

Date: **2026-09-07**

Status: **WAVE 1C-A / 1C-B IMPLEMENTATION UPDATED; BUILD + AUTOMATION + PIE REVALIDATION REQUIRED BEFORE SEAL**

## Purpose

Wave 1C introduces the generic selection primitive required by cards whose resolution depends on player choice.

Wave 1B only establishes targeted exhaust primitive. Player-driven selection must not be merged into Wave 1B consumers.

The goal of Wave 1C is to define a reusable gameplay-owned selection flow and close the first playable consumer path with Burning Pact-shaped composition.

---

## Ownership Contract

Selection state belongs to Gameplay authority.

```text
Gameplay Action / Resolver
        |
        | Create SelectionRequest
        v
SelectionResolver
        |
        | UI-safe read/request facade
        v
Native HUD / ViewModel
        |
        | RuntimeId-only player input
        v
Gameplay selection request facade
        |
        | Build validated SelectionResult
        v
SelectionResolver
        |
        | Resume awaiting Action
        v
BattleActionQueue continuation
```

### Forbidden ownership

Presentation layer must not:

- store authoritative gameplay pending-selection objects
- decide valid candidates
- mutate cards/zones
- directly execute gameplay continuation
- enqueue authoritative BattleActions

The Native HUD/ViewModel is only an input submission surface. The Wave 1C bridge exposes stable card RuntimeIds and cancelability, never authoritative candidate `UObject*` pointers.

---

## Selection Request Contract

A SelectionRequest contains:

- selection source
- candidate runtime objects
- minimum / maximum selection count
- request-level cancellation policy
- authored continuation passed alongside the request into the resolver

Current cancellation policy:

```text
ESelectionCancelPolicy::Allowed
  -> generic legal cancel path

ESelectionCancelPolicy::Forbidden
  -> mandatory choice; cancel is rejected and the awaiting Action remains pending
```

> The earlier "continuation identifier" wording is superseded by the actual
> Wave 1C-A implementation. `FSelectionRequest` carries no string continuation
> identifier; the typed, stateless `UAuthoredContinuation` is passed alongside
> the request into `USelectionResolver::BeginSelection`.

The request is created by Gameplay before Presentation interaction begins.

---

## Selection Result Contract

SelectionResult is accepted only through Gameplay validation.

Validation includes:

- selected object exists
- selected object belongs to candidate set
- selection count satisfies rules
- runtime state is still valid

Invalid selection must not mutate gameplay state.

For the Native single-card Hand bridge, Presentation submits only a RuntimeId. `BattleSelectionRequest` resolves that RuntimeId back to the exact pending Gameplay candidate and then constructs `FSelectionResult` inside Gameplay.

---

## Continuation Resume Contract

Continuation execution belongs to Gameplay queue flow.

```text
SelectionResult
      |
      v
Gameplay Resolver
      |
      v
Resume awaiting SelectionRequestAction
      |
      v
Authored Continuation builds dependent Actions
      |
      v
BattleActionQueue
```

Forbidden:

```text
Widget callback
      |
      v
Direct Gameplay Action execution
```

Presentation cannot bypass BattleActionQueue ordering.

---

## Cancel / Invalid Semantics

### Allowed cancel

When the authored request uses `ESelectionCancelPolicy::Allowed`:

- cancellation is a legal resolution path
- clear pending selection
- do not mutate gameplay state
- do not emit ResolutionFault
- finish the waiting SelectionRequestAction and resume the queue

### Forbidden cancel

When the authored request uses `ESelectionCancelPolicy::Forbidden`:

- `SubmitCancel()` returns false
- a submitted `Cancelled` result is rejected
- pending request remains active
- awaiting SelectionRequestAction does not `Finish()`
- later queued Effects cannot continue
- no gameplay mutation and no ResolutionFault

This prevents mandatory card costs such as Burning Pact's exhaust step from being skipped while a later Draw Effect still resolves.

### Invalid

Invalid selection is a contract violation.

Examples:

- runtime id missing
- object not in candidate list
- invalid selection count

Rules:

- no gameplay mutation
- emit controlled failure information
- preserve deterministic state

The production Native Hand bridge does not submit arbitrary object pointers; an invalid/non-candidate RuntimeId is rejected before a `SelectionResult` is constructed.

---

## Native HUD input bridge

Wave 1C now closes the playable input gap without reopening the sealed A2/A3 ownership model.

```text
UBattleCardWidget click
→ UBattleHUDWidget::SelectCard(RuntimeId)
→ if no pending Gameplay selection:
     unchanged normal card-play / fast-presentation path
→ if a pending single-card Gameplay selection exists:
     UBattleHUDViewModel::SubmitPendingCardSelectionByRuntimeId
     → BattleSelectionRequest::SubmitPendingCardSelection
     → USelectionResolver::SubmitResult
```

Important boundaries:

```text
- ordinary Resolving input remains locked
- only an actual pending single-card request activates the alternate click route
- UI never receives candidate UObject pointers
- non-candidate RuntimeIds are rejected
- historical A2 display remains frozen until normal committed Presentation catches up
```

---

## Wave 1C Scope Split

### Wave 1C-A — Selection Primitive

Implements:

- SelectionRequest / SelectionResult
- SelectionResolver
- request-level cancel policy
- queue continuation contract
- generic allowed-cancel behavior
- automation coverage

### Wave 1C-B — First Consumer / playable bridge

Uses Burning Pact as the first consumer shape and implements:

- reusable `USelectExhaustHandCardEffect`
- mandatory select-one-Hand-card semantics
- `UExhaustSelectedContinuation`
- Native HUD RuntimeId-only pending card selection route
- transient full Burning Pact Automation shape (`SelectExhaust -> Draw`)

### Wave 1C-C — Expansion

Future consumers may reuse the primitive:

- True Grit upgraded behavior
- Exhume style selection
- other player-choice cards
- future multi-select UI where a real consumer requires it

---

## Validation Gate

Required automation coverage after the 2026-09-07 closure changes:

- request creation
- candidate generation
- valid selection
- invalid selection rejection
- allowed cancel path
- mandatory cancel rejection while queue remains held
- continuation resume ordering
- Burning Pact base: exhaust one -> draw 2 -> FinishCardPlay
- Burning Pact upgraded: exhaust one -> draw 3 -> FinishCardPlay
- Effects order is `[SelectExhaust, Draw]`
- Native HUD C++ card-click route submits a pending candidate while the ActionQueue is intentionally busy

Required manual validation:

- Editor Development build
- focused `SlayTheSpireDemo.CardExpansion.Wave1C` Automation
- Native production HUD PIE: play the test Burning Pact card, click another Hand card, observe exact exhaust then draw and normal return to input

The previous 7/7 result predates these closure changes and is historical evidence only. The updated implementation must be re-run before Wave 1C is marked validated/sealed again.

---

## Wave 1C-B Design Details (locked)

### Delivery shape

Wave 1C-B delivers a **reusable orthogonal CardEffect**, not a hard-coded card. A concrete card (`UCardData`) remains an authored `Effects[]` array composed of these Effects.

The repository branch may contain owner-authored `DA_Card_BurningPact.uasset` / `L_BattleTest.umap` changes for ad-hoc testing. Those binary assets are **not part of this C++ closure change, are not inspected or modified here, and are not accepted as Wave 1C production validation evidence**. Production asset authoring/acceptance remains a separate user-owned step.

### New Effect — select-and-exhaust

A single composable Effect owns the "select a Hand card, then exhaust it" capability:

```text
USelectExhaustHandCardEffect
  candidates = current Hand cards minus the played card (computed in BuildActions)
  behavior:
    - has burnable cards  -> mandatory select one -> exhaust it
    - no burnable cards   -> skip burning entirely (no selection, no exhaust, no fault)
```

Candidate timing:

```text
Candidates are computed in BuildActions as "current Hand cards, excluding the
played card (Context.Card)". This is deterministic regardless of whether the
played card has physically left Hand yet.

has burnable candidates
  -> enqueue USelectionRequestAction(candidates, min=1, max=1,
     CancelPolicy=Forbidden)

no burnable candidates
  -> skip burning entirely
```

### Selection mode

Burning Pact uses manual player choice. Random selection remains deferred until a real random-exhaust consumer requires it.

```text
Manual  -> USelectionRequestAction + Native pending-card input bridge   [Wave 1C]
Random  -> deterministic battle RNG                                    [deferred]
```

### Determinism requirement for future Random mode

```text
Random selection MUST use the battle-local deterministic RNG stream. No
per-frame, timer, or fresh non-deterministic random source is allowed.
```

Current battle RNG remains `UDeckRuntime::RandomStream`; expose the smallest shared battle-scoped helper only when Random selection becomes an authorized consumer requirement.

### STS reference

```text
Burning Pact: "Exhaust 1 card. Draw 2 (3) cards."
```

The exhausted card is chosen by the player; the choice is mandatory when a valid candidate exists.

### Ordering is authored, not baked into the Effect

```text
Burning Pact = [
  USelectExhaustHandCardEffect,    // mandatory select one -> exhaust
  UDrawCardEffect(DrawCount 2/3)   // then draw
]
```

`PlayCardAction` authors these Effects in order. `USelectionRequestAction` holds the queue; resolving the selection inserts the exhaust continuation before the already-authored Draw Action. Draw therefore cannot run before the mandatory selection completes.

### Exhaust / Draw remain orthogonal

```text
USelectionRequestAction  -> does not know exhaust details
UExhaustCardAction       -> does not know who selected it or that draw follows
UDrawCardsAction         -> does not know what preceded it
USelectExhaustHandCardEffect -> composition point connecting selection -> exhaust
```

### Numerical content used by transient Automation

```text
Burning Pact:  BaseCost 1; exhaust 1 Hand card; draw 2
Burning Pact+: Cost 1;     exhaust 1 Hand card; draw 3
DefaultDestination = Discard
```

### Not implemented / not sealed by this slice

```text
- no acceptance of repository-local test .uasset/.umap as production content
- no Random selection mode
- no selection-into-other-moves (Exhume / True Grit selection)
- no bulk exhaust (Fiend Fire / Second Wind)
- no reactive exhaust powers (Feel No Pain / Dark Embrace)
- no generic multi-select Native UI before a real consumer requires it
```
