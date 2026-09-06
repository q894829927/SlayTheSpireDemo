# Card Expansion — Wave 1B Execution Record

Date: **2026-09-06**

Status:

```text
COMPLETE / VALIDATED / SEALED
```

Branch:

```text
main
```

Authority:

```text
docs/CardExpansionWave1BTargetedExhaustPrimitive.md
```

Dependency status:

```text
Wave 1A source implementation     PRESENT
Wave 1A validation                COMPLETED
Wave 1B source implementation     PRESENT
Wave 1B focused validation         COMPLETED
```

Wave 1B consumes the CardExhausted source contract and adds the targeted exhaust primitive without changing DeckRuntime ownership boundaries.

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

## Focused Automation validation

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

Result:

```text
PASS
```

Coverage:

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

## Seal gate

Completed:

```text
[X] Editor Development Build PASS
[X] focused Wave 1B Automation PASS
[X] source review of Action result/dispatch ordering PASS
[X] final validation evidence recorded
[X] Final user seal confirmation (2026-09-06)
```

No Wave 1C implementation is included in this slice.
