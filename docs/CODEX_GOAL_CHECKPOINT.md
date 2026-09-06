# Codex Goal Checkpoint — Production Card Expansion

Last updated: **2026-09-06**

## Current status

```text
Phase 6UI-A / A3:
COMPLETE / VALIDATED / SEALED

Phase 7A–7F:
COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
COMPLETE / VALIDATED / SEALED

Card Face Visual Style (CFV):
COMPLETE / USER-ACCEPTED / SEALED

Production Card Expansion:
ACTIVE

Wave 1A — Exhaust Fact Surface:
SOURCE IMPLEMENTATION PRESENT / VALIDATION PENDING / NOT SEALED

Wave 1B — Targeted Exhaust Primitive:
AUTHORIZED / IMPLEMENTATION IN PROGRESS / VALIDATION DEFERRED BY USER
```

## Current branch

```text
card-expansion-wave1b-targeted-exhaust
```

Branch base:

```text
main@57ea2dabaee80a9a9048869a9c84c066c08b2e13
```

The user explicitly authorized starting Wave 1B before Wave 1A validation. This changes scheduling only. It does not upgrade Wave 1A to COMPLETE / VALIDATED / SEALED and does not authorize Wave 1C or later slices.

---

## Current authority chain

Long-term Ironclad architecture:

```text
docs/IroncladCardArchitecturePlan.md
```

Wave-1 ordering amendment:

```text
docs/IroncladCardArchitecturePlanWave1Amendment.md
```

Wave 1A authority / source record:

```text
docs/CardExpansionWave1AExhaustFactSurface.md
docs/CardExpansionWave1AExecution.md
```

Current Wave 1B authority / execution record:

```text
docs/CardExpansionWave1BTargetedExhaustPrimitive.md
docs/CardExpansionWave1BExecution.md
```

Sealed ordinary-card upgrade authority:

```text
docs/CardUpgradeSTSStyleRefactor.md
```

Future independent Card trigger-source design:

```text
docs/CardTriggerSourceExpansionDesign.md
```

When old scheduling language conflicts with this checkpoint, the explicit Wave 1B user authorization recorded here wins only for Wave 1B.

---

## Current production card baseline

Production CardData remains six validated assets:

```text
Attack
- Strike
- Pommel Strike
- Twin Strike
- Uppercut

Skill
- Defend

Power
- Inflame
```

Seeing Red is still a pending Wave 1A production validation asset and is not part of the validated production baseline yet.

---

## Wave 1A dependency state

Wave 1A source currently provides:

```text
EBattleEventType::CardExhausted
FCardExhaustedEvent
FBattleEvent::MakeCardExhausted(...)
self-exhaust producer in UFinishCardPlayAction
explicit Dispatcher/combatant propagation
UGainEnergyCardEffect
focused Wave 1A Automation source
```

Wave 1A evidence still pending:

```text
Editor Build
focused Automation execution
Seeing Red production asset
production DataAsset validation
PIE acceptance
final seal record
```

Wave 1B may consume the present CardExhausted source contract provisionally, but no document may claim that dependency is sealed until those gates actually pass.

---

## Wave 1B locked scope

Wave 1B establishes only:

```text
already-specified exact UCardInstance* currently in Hand
→ UDeckRuntime::TryExhaustHandCardCommit
→ exact FCardZoneMutationResult
→ UExhaustCardAction
→ committed CardZoneChanged Presentation record when available
→ same FCardExhaustedEvent contract
→ Dispatcher
```

The DeckRuntime commit is intentionally:

```text
Hand → ExhaustPile
```

It is not a universal AnyZone mutation API.

`UExhaustCardAction` retains:

```cpp
const FCardZoneMutationResult& GetCommitResult() const;
```

This typed result surface exists for a future Wave 1C authored Continuation. Wave 1B does not implement that Continuation.

---

## Wave 1B producer / failure rules

Required ordering:

```text
exact target still in Hand
→ validate explicit event wiring
→ authoritative Hand → ExhaustPile commit
→ retain exact typed CommitResult
→ verify exact target identity / source / destination facts
→ CardZoneChanged Presentation record when available
→ CardExhausted from held Card + the same CommitResult
→ Dispatch with current Presentation writer
```

Failure semantics:

```text
stale/non-Hand target
→ fail-soft
→ no commit
→ no CardExhausted

missing event wiring
→ ResolutionFault before commit

Dispatch failure after successful commit
→ ResolutionFault
→ committed Exhaust remains authoritative
→ no rollback
```

DeckRuntime remains unaware of Dispatcher and does not emit Gameplay events itself.

---

## Wave 1B focused Automation source

Authored prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust
```

Cases currently authored:

```text
CommitAndEvent
WiringFailureBeforeCommit
DeckRuntimeMutationOnly
```

Coverage intent:

```text
exact specified target only
Hand → ExhaustPile exact CommitResult
another Hand card untouched
Action typed result matches commit
exactly one CardExhausted
Event payload matches CommitResult
Presentation CardZoneChanged matches CommitResult
commit observed before dispatch
stale retry produces no duplicate event
missing wiring faults before mutation
DeckRuntime-only commit dispatches no event
```

Per user instruction these tests are not being run yet. No PASS claim is authorized.

---

## Explicit Wave 1B non-goals

```text
SelectionRequest / SelectionResult
Burning Pact full card
True Grit selection path
Fiend Fire bulk exhaust
Second Wind bulk/filter exhaust
bulk Exhaust Action
Exhaust CardEffect that discovers/chooses targets
generic authored Continuation
arbitrary-zone targeted exhaust
Feel No Pain
Dark Embrace
Sentinel
Card Trigger Source Expansion
Ethereal
multi-enemy
Phase 8 implementation
new UI
```

---

## Revised active path

```text
Wave 1A — Exhaust Fact Surface
→ SOURCE PRESENT / VALIDATION PENDING

Wave 1B — Targeted Exhaust Primitive
→ CURRENT ACTIVE BRANCH SLICE
→ SOURCE IMPLEMENTATION IN PROGRESS
→ VALIDATION DEFERRED

Wave 1C — Selection + Targeted Exhaust Composition
→ FUTURE / NOT AUTHORIZED

Wave 1D — Reactive Exhaust Powers
→ FUTURE / NOT AUTHORIZED

Card Trigger Source Expansion
→ FUTURE INDEPENDENT FOUNDATION / NOT AUTHORIZED
```

---

## Current stop point

Wave 1B source work may continue only until the dedicated 1B authority stop point is satisfied:

```text
DeckRuntime Hand-targeted Exhaust commit
UExhaustCardAction
typed exact result accessor
CardZoneChanged projection
same CardExhausted dispatch contract
focused Automation source
execution record
```

Do not start Wave 1C from this authorization. Do not claim Wave 1A or Wave 1B sealed without actual validation evidence.
