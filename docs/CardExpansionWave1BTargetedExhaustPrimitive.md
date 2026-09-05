# Card Expansion — Wave 1B Targeted Exhaust Primitive

Date: **2026-09-06**

Status:

```text
AUTHORIZED
IMPLEMENTATION IN PROGRESS
VALIDATION DEFERRED BY USER
```

Branch:

```text
card-expansion-wave1b-targeted-exhaust
```

Base:

```text
main@57ea2dabaee80a9a9048869a9c84c066c08b2e13
```

Upstream authorities:

```text
docs/IroncladCardArchitecturePlan.md
docs/IroncladCardArchitecturePlanWave1Amendment.md
docs/CardExpansionWave1AExhaustFactSurface.md
```

Wave 1A source implementation is present on the branch base, but its Build / Automation / production asset / PIE validation is still pending. Wave 1B must not describe Wave 1A as COMPLETE / VALIDATED / SEALED.

---

## 1. Goal

Wave 1B establishes the smallest explicit targeted-exhaust primitive needed by later authored card composition:

```text
an already-specified exact UCardInstance* currently in Hand
→ authoritative Hand → ExhaustPile mutation
→ exact typed FCardZoneMutationResult
→ committed CardZoneChanged Presentation record when available
→ the same CardExhausted committed-event rule introduced by Wave 1A
```

The slice does not decide which card should be exhausted. Selection and card-specific candidate rules remain outside the primitive.

Core rule:

> Targeted Exhaust receives an exact card target; it does not discover or choose that target.

---

## 2. Source-zone scope — intentionally narrow

The first real targeted-exhaust consumers are Hand-based mechanics such as Burning Pact, True Grit, Fiend Fire and Second Wind.

Therefore Wave 1B adds only:

```text
Hand → ExhaustPile
```

Do not introduce a universal arbitrary-zone move API such as:

```text
MoveCardFromAnyZoneToAnyZone(...)
UniversalZoneMutation(...)
```

Future Discard / Draw / Exhaust / Removed-source movement must be added only when a real card mechanic requires it.

Recommended DeckRuntime commit boundary:

```cpp
FCardZoneMutationResult TryExhaustHandCardCommit(UCardInstance* Card);
```

`UDeckRuntime` remains the authoritative zone owner and does not know BattleEventDispatcher.

---

## 3. Action primitive

Add:

```text
UExhaustCardAction
```

The Action receives explicit dependencies:

```text
UDeckRuntime*
exact UCardInstance*
PresentationCardSource
BattleEventDispatcher*
authoritative Combatants
```

It must not:

```text
search the World for BattleManager / Dispatcher
select a card
filter a candidate set
know Burning Pact / True Grit / Fiend Fire / Second Wind
inspect CardId / DisplayName for behavior
```

Execution order:

```text
validate Action / Deck / exact Card
validate event-dispatch wiring before mutation
→ DeckRuntime::TryExhaustHandCardCommit(Card)
→ receive exact FCardZoneMutationResult
→ if not committed: Finish with no event
→ verify exact Card identity + Hand → ExhaustPile facts
→ append CardZoneChanged Presentation record when writer is available
→ dispatch FCardExhaustedEvent from held Card + the same commit result
→ Finish
```

If Dispatch fails after a successful commit:

```text
RequestResolutionFault
→ do not roll back committed Exhaust
→ Finish
```

---

## 4. Typed result surface for future authored Continuation

Wave 1C will need to distinguish:

```text
selected card actually exhausted
→ dependent follow-up may be built

selected card no longer exhaustible at Execute-time
→ dependent follow-up must not be granted automatically
```

Therefore `UExhaustCardAction` must retain its exact execution result and expose a narrow typed read surface:

```cpp
const FCardZoneMutationResult& GetCommitResult() const;
```

Rules:

```text
Initialize / pre-Execute
→ default result with bCommitted=false

successful Execute
→ exact DeckRuntime commit result retained unchanged

failed / stale target
→ bCommitted=false
```

This does not implement a Continuation system. It only makes the primitive's exact typed result available to a later explicitly authorized authored Continuation.

Do not add:

```text
UniversalResultBus
string-key result lookup
mutable property bag
card-specific success flags
```

---

## 5. CardExhausted event reuse

Wave 1B is the second real Exhaust producer:

```text
Wave 1A producer
PlayArea → ExhaustPile

Wave 1B producer
Hand → ExhaustPile
```

Both must produce the same neutral fact shape:

```text
held UCardInstance* subject
+ exact FCardZoneMutationResult
→ FCardExhaustedEvent
```

The Wave 1B event must report:

```text
FromZone == Hand
ToZone   == ExhaustPile
```

because those values came from the commit result, not because the event schema hardcodes Hand.

Wave 1B does not add new event fields or a second Exhaust event type.

A shared helper may be extracted only if the two concrete producers show useful duplication after implementation. Do not create a universal zone-event framework.

---

## 6. Presentation ordering

Keep the established ordering:

```text
authoritative mutation
→ exact commit result
→ CardZoneChanged Presentation record when available
→ CardExhausted dispatch with the same current Presentation writer
```

Gameplay remains authoritative if Presentation append/freeze fails.

No new Presentation record type is required.

---

## 7. Explicit non-goals

Wave 1B does not implement:

```text
SelectionRequest / SelectionResult
Burning Pact full card
True Grit selection behavior
Fiend Fire bulk exhaust
Second Wind bulk/filter composition
bulk exhaust action
Exhaust CardEffect that chooses targets
generic authored Continuation
Feel No Pain
Dark Embrace
Sentinel / Card Trigger Source Expansion
Ethereal
Discard-pile targeted exhaust
Draw-pile targeted exhaust
universal arbitrary-zone mutation
new UI
```

Wave 1C owns Selection + Targeted Exhaust composition.

---

## 8. Focused Automation source

Even though the user has temporarily deferred running validation, implementation should include focused Automation source covering:

```text
exact specified Hand card → Hand → ExhaustPile commit
GetCommitResult() matches the exact mutation
exactly one CardExhausted event
CardExhausted payload matches CommitResult
CardZoneChanged matches CommitResult
commit occurs before dispatch
missing event wiring → fault before mutation
card not in Hand → no commit / no event
second Execute / stale exact card → no duplicate event
another Hand card remains untouched
```

Suggested prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust
```

No PASS claim may be recorded until the tests are actually run.

---

## 9. Stop point

Wave 1B source implementation stops when these are present:

```text
DeckRuntime Hand-targeted Exhaust commit
UExhaustCardAction
exact typed result accessor
CardZoneChanged projection
same CardExhausted event dispatch contract
focused Automation source
execution record
```

Do not begin Wave 1C from this authorization.