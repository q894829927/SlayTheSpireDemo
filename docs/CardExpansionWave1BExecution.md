# Card Expansion — Wave 1B Execution Record

Date: **2026-09-06**

Status:

```text
IMPLEMENTATION IN PROGRESS
SOURCE IMPLEMENTATION PRESENT
VALIDATION NOT RUN — DEFERRED BY USER
```

Branch:

```text
card-expansion-wave1b-targeted-exhaust
```

Authority:

```text
docs/CardExpansionWave1BTargetedExhaustPrimitive.md
```

Dependency status:

```text
Wave 1A source implementation     PRESENT ON BRANCH BASE
Wave 1A Build / Automation / PIE PENDING
Wave 1A seal                      NOT CLAIMED
```

Wave 1B therefore consumes the current CardExhausted source contract provisionally and does not change Wave 1A status.

---

## Implemented source surface

Deck mutation:

```text
UDeckRuntime::TryExhaustHandCardCommit(UCardInstance*)
```

Contract:

```text
exact CardInstance must currently be in Hand
→ Hand.RemoveAt(exact index)
→ ExhaustPile.Add(exact instance)
→ FCardZoneMutationResult
   bCommitted = true
   CardRuntimeId / CardId = exact target
   FromZone = Hand
   ToZone = ExhaustPile
   exact FromIndex / ToIndex
```

No Dispatcher/Event dependency was added to DeckRuntime.

Action primitive:

```text
UExhaustCardAction
```

Explicit dependencies:

```text
Deck
exact CardInstance
PresentationCardSource
BattleEventDispatcher
combatant context
```

Execution:

```text
stale/non-Hand exact target
→ fail-soft / no commit / no event

valid exact Hand target
→ validate dispatch wiring
→ TryExhaustHandCardCommit
→ retain exact CommitResult
→ verify identity + Hand → ExhaustPile facts
→ CardZoneChanged Presentation record when available
→ FCardExhaustedEvent from held Card + same CommitResult
→ Dispatch with current Presentation writer
```

Dispatch failure after commit requests ResolutionFault and does not roll back the committed Exhaust.

---

## Typed result surface

`UExhaustCardAction` exposes:

```cpp
const FCardZoneMutationResult& GetCommitResult() const;
```

This is intentionally narrow. It exists so a later Wave 1C authored Continuation can distinguish successful Exhaust from a stale/failed target without a universal result bus.

Wave 1B does not implement Continuation execution or card-selection orchestration.

---

## Focused Automation source added

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust
```

Cases:

```text
CommitAndEvent
WiringFailureBeforeCommit
DeckRuntimeMutationOnly
```

Coverage intent:

```text
exact specified Hand card commits to ExhaustPile
another Hand card remains untouched
Action CommitResult matches exact mutation
exactly one CardExhausted event
Event payload matches CommitResult
CardZoneChanged record matches CommitResult
Dispatch observes already-committed state
stale retry emits no duplicate event
missing wiring faults before commit
DeckRuntime mutation alone does not dispatch Gameplay events
```

These tests have been authored but not executed.

---

## Explicitly not implemented

```text
SelectionRequest / SelectionResult
Burning Pact full card
True Grit upgraded selection
Fiend Fire / Second Wind bulk exhaust
bulk Exhaust Action
arbitrary-zone targeted exhaust
Exhaust CardEffect that discovers a target
generic authored Continuation
Feel No Pain / Dark Embrace
Sentinel / Card Trigger Source Expansion
new UI
```

---

## Pending before any seal claim

Per current user instruction, validation is intentionally deferred.

Still required later:

```text
[ ] Editor Development Build PASS
[ ] Wave 1A validation completed or dependency explicitly accepted
[ ] focused Wave 1B Automation PASS
[ ] source review of Action result/dispatch ordering PASS
[ ] final validation evidence recorded
[ ] Wave 1B status advanced only after actual evidence
```

No COMPLETE / VALIDATED / SEALED claim is made by this record.
