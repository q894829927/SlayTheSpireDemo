# Card Expansion — Wave 1C Selection Primitive

Date: **2026-09-06**

Status: **DESIGN LOCKED / IMPLEMENTATION AUTHORITY**

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
- continuation identifier

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
