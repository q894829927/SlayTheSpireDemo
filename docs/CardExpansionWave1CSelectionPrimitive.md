# Card Expansion — Wave 1C Selection Primitive

Date: **2026-09-06**

Status: **WAVE 1C-A COMPLETE / VALIDATED / READY FOR SEAL; 1C-B COMPLETE / VALIDATED / READY FOR SEAL**

## Purpose

Wave 1C introduces the generic selection primitive required by cards whose resolution depends on player choice.

Wave 1B only establishes targeted exhaust primitive. Player-driven selection must not be merged into Wave 1B consumers.

The goal of Wave 1C is to define a reusable gameplay-owned selection flow before implementing individual consumers such as Burning Pact.

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
        | Presentation request
        v
Widget / UI
        |
        | User input result
        v
SelectionResolver
        |
        | Validate + produce SelectionResult
        v
BattleActionQueue continuation
```

### Forbidden ownership

Presentation layer must not:

- store gameplay pending selection state
- decide valid candidates
- mutate cards/zones
- directly execute gameplay continuation

Widget is only an input submission surface.

---

## Selection Request Contract

A SelectionRequest contains:

- selection source
- candidate runtime objects
- minimum / maximum selection count
- validation rules
- authored continuation (the `UAuthoredContinuation` definition object)

> Note: the earlier "continuation identifier" wording is superseded by the actual
> Wave 1C-A implementation. `FSelectionRequest` carries no string identifier;
> the typed, stateless `UAuthoredContinuation` is passed alongside the request
> into `USelectionResolver::BeginSelection`, matching the locked §2.6 contract.

The request is created by Gameplay before Presentation interaction begins.

---

## Selection Result Contract

SelectionResult is produced only after Gameplay validation.

Validation includes:

- selected object exists
- selected object belongs to candidate set
- selection count satisfies rules
- runtime state is still valid

Invalid selection must not mutate gameplay state.

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
Resume authored continuation
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

### Cancel

Cancel is a valid resolution path.

Rules:

- clear pending selection
- do not mutate gameplay state
- do not emit ResolutionFault
- queue returns to defined waiting/resume state

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

---

## Wave 1C Scope Split

### Wave 1C-A — Selection Primitive

Implement only:

- SelectionRequest
- SelectionResolver
- SelectionResult
- queue continuation contract
- automation coverage

### Wave 1C-B — First Consumer

Use Burning Pact as the first consumer.

Do not implement multiple consumers before the primitive is validated.

### Wave 1C-C — Expansion

Future consumers may reuse the primitive:

- True Grit upgraded behavior
- Exhume style selection
- other player-choice cards

---

## Validation Gate

Required automation coverage:

- request creation
- candidate generation
- valid selection
- invalid selection rejection
- cancel path
- continuation resume ordering

Wave 1C implementation is not considered complete until ownership boundaries and deterministic continuation behavior are verified.

---

## Wave 1C-B Design Details (locked)

This section records the design decisions locked for Wave 1C-B (Burning Pact as the first consumer) before implementation.

### Delivery shape

Wave 1C-B delivers a **reusable orthogonal CardEffect**, not a hard-coded card. A concrete card (`UCardData`) is just an authored `Effects[]` array composed of these Effects. Production `DA_Card_BurningPact` asset authoring is deferred to the user in Unreal Editor.

### New Effect — select-and-exhaust

A single composable Effect owns the "select a Hand card, then exhaust it" capability:

```text
USelectExhaustHandCardEffect
  candidates = current Hand cards minus the played card (computed in BuildActions)
  behavior:
    - has burnable cards  -> select one -> exhaust it
    - no burnable cards   -> skip burning entirely (no selection, no exhaust, no fault)
```

Candidate timing (important):

```text
Candidates are computed in BuildActions as "current Hand cards, excluding the
played card (Context.Card)". This is deterministic and correct regardless of
whether the played card has physically left Hand yet, because the Effect knows
Context.Card directly and simply skips it when enumerating.

  has burnable candidates (hand minus played card is non-empty)
      -> enqueue USelectionRequestAction(candidates, min=1, max=1)
  no burnable candidates (hand minus played card is empty)
      -> skip burning entirely (no selection, no exhaust, no fault)
```

Selection mode (deferred): Burning Pact uses manual player choice. The reusable
`ESelectMode` enum with a `Random` branch is intentionally NOT authored in 1C-B;
it will be introduced only when a real random-exhaust consumer (e.g. Fiend Fire
style) needs it.

```text
Manual  -> USelectionRequestAction (pause for player choice; Gameplay-owned)   [1C-B]
Random  -> deterministic battle RNG picks the card                            [deferred, not in 1C-B]
```

### Determinism requirement

```
Random selection MUST use the battle-local deterministic RNG stream (reproducible
for the same initial state + input sequence + RNG seed). No per-frame, timer, or
non-deterministic random source is allowed.
```

### RNG access point (implementation TODO)

```text
Current battle-scoped deterministic RNG is UDeckRuntime::RandomStream
(FRandomStream, initialized from InitialSeed in InitializeFromDefinitions; used
by ShuffleDrawPileWithBattleRng via RandomStream.RandRange). It is currently a
private member. Before implementing Random selection, add the smallest read/use
surface on UDeckRuntime (e.g. a battle-scoped rand-range helper) so the Effect
consumes the SAME stream, never a fresh or per-frame random source.
```

### STS reference (Burning Pact selection is manual, not random)

```text
STS Burning Pact: "Exhaust 1 card. Draw 2 (3) cards." — the exhausted card is
chosen BY THE PLAYER. Random selection exists only as a reusable mode for future
cards, not for Burning Pact.
```

### Ordering is authored, not baked into the Effect

```
The Effect does not decide whether exhaust or draw happens first. Ordering comes
from the card's Effects[] array.

Burning Pact = [
  USelectExhaustHandCardEffect,    // select one -> exhaust (skip if none)
  UDrawCardEffect(DrawCount 2/3)   // then draw
]
```

### Exhaust / Draw remain orthogonal

```
Action-layer primitives stay orthogonal:

  USelectionRequestAction  -> does not know exhaust details (only "select a thing")
  UExhaustCardAction       -> does not know who selected it, nor that draw follows
  UDrawCardsAction         -> does not know what preceded it
```

The composable `USelectExhaustHandCardEffect` IS the authored composition point:
it knows both "select which card" and "exhaust the selected card", and bridges
them locally. This is exactly the allocation described in
`IroncladCardArchitecturePlan.md` §2.3 — the effect/composition layer may know
multiple public capability contracts, while the primitive Actions stay neutral.

```text
Select   (Action primitive)   -> neutral; doesn't know exhaust
Exhaust  (Action primitive)   -> neutral; doesn't know selection or draw
Draw     (Action primitive)   -> neutral
USelectExhaustHandCardEffect  -> composition point that connects selection -> exhaust
```

The Effect is the composition point at the same level as the existing
`UDamageCardEffect` / `UDrawCardEffect` / `UGainEnergyCardEffect`, and may be
reused by future cards (e.g. Fiend Fire-style random exhaust).

### Numerical content (STS canon)

```text
Burning Pact: BaseCost 1; exhaust 1 Hand card; draw 2
Burning Pact+: BaseCost 1; exhaust 1 Hand card; draw 3
```

### Not implemented in 1C-B

```text
- no production DA_Card_BurningPact asset (user-authored later)
- no Random selection mode (ESelectMode.Random is deferred; only Manual in 1C-B)
- no selection-into-other-moves (Exhume / True Grit selection)
- no bulk exhaust (Fiend Fire / Second Wind)
- no reactive exhaust powers (Feel No Pain / Dark Embrace)
- no multi-consumer generalization before the first consumer is validated
```
