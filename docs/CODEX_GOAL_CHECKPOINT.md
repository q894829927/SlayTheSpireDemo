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
DESIGN LOCKED / IMPLEMENTATION IN PROGRESS / VALIDATION PENDING
```

## Current branch

```text
main
```

Wave 1A implementation continues directly on `main` unless the user explicitly changes the workflow.

---

## Current authority chain

Long-term Ironclad card architecture:

```text
docs/IroncladCardArchitecturePlan.md
```

Current ordering amendment:

```text
docs/IroncladCardArchitecturePlanWave1Amendment.md
```

Current Wave 1A dedicated authority:

```text
docs/CardExpansionWave1AExhaustFactSurface.md
```

Sealed ordinary-card upgrade authority:

```text
docs/CardUpgradeSTSStyleRefactor.md
```

Sealed card-face authority/evidence:

```text
docs/CardFaceVisualStyleImplementation.md
docs/CFV1Validation.md
docs/CFV2CardFaceShellExecution.md
docs/CFV3StyleSetResolverExecution.md
docs/CFV4ProductionStyleSetExecution.md
docs/CFV5VisualAcceptance.md
```

Future independent Card trigger-source design:

```text
docs/CardTriggerSourceExpansionDesign.md
```

The current ordering amendment supersedes stale scheduling language in older planning docs when it conflicts with this checkpoint. It does not silently expand implementation scope.

---

## Current production card baseline

Current production CardData set remains 6 assets:

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

Seeing Red is the planned Wave 1A production validation asset and is not yet counted in this baseline until the `.uasset` is authored and validated.

Ordinary upgrade support for current typed Damage / Block / Draw / ApplyStatus effect fields is already sealed. Wave 1A adds the narrow typed GainEnergy CardEffect adapter without reopening the upgrade model.

---

## Exhaust capability split

Do not use “Exhaust is implemented” without qualification.

### Existing before Wave 1A: self-exhaust after play

```text
UCardData::DefaultDestination = Exhaust
→ normal play lifecycle
→ FinishCardPlay
→ PlayArea → ExhaustPile authoritative commit
```

### Wave 1A source implementation now in progress

The current Wave 1A C++ work establishes:

```text
successful self-exhaust commit
→ exact immutable FCardExhaustedEvent
→ BattleEventDispatcher
```

Current producer ownership remains:

```text
UPlayCardAction
→ explicitly passes battle-scoped Dispatcher + combatants
→ UFinishCardPlayAction
→ authoritative DeckRuntime destination commit
→ committed CardZoneChanged Presentation record when available
→ FCardExhaustedEvent from held Card + exact commit result
→ Dispatcher
```

`DeckRuntime` remains unaware of Dispatcher/Event orchestration.

### Deferred: targeted exhaust

```text
select/specify exact CardInstance
→ explicit Exhaust mutation/action
```

Needed later by Burning Pact / Fiend Fire / Second Wind / related cards.

Targeted exhaust is not part of Wave 1A.

---

## Wave 1A locked producer boundary

Required ordering:

```text
FinishCardPlay / current card-play composition boundary
→ validate Exhaust event wiring before Exhaust commit
→ request zone move commit
→ receive exact typed commit result
→ existing committed Presentation record when available
→ if committed && ToZone == ExhaustPile
   → build FCardExhaustedEvent from held Card + committed result
   → dispatch with the current Presentation writer
```

Failure semantics:

```text
invalid Exhaust event wiring
→ fault before Exhaust commit

successful Exhaust commit + Dispatch failure
→ RequestResolutionFault
→ do not rollback committed Exhaust
```

Forbidden:

```text
DeckRuntime directly dispatches BattleEvent
DefaultDestination == Exhaust → assume commit success
dispatch before mutation
CardId / DisplayName special case
world/actor search for Dispatcher
```

The event describes a committed fact, not play intent. Its scalar identity/zone fields come from the exact commit result; the runtime Card pointer comes from the producer's already-held Card reference.

---

## Wave 1A production validation card

Default validation card:

```text
Seeing Red
```

Locked authored values:

```text
BaseCost       = 1
UpgradedCost   = 0
BaseAmount     = 2
UpgradedAmount = 2
DefaultDestination = Exhaust
```

`UGainEnergyAction` already exists. Wave 1A now adds `UGainEnergyCardEffect`, which only resolves typed Base/Upgraded authored values and builds the existing Action. It does not implement a fake ImmediatePreview operation and contains no Seeing Red/CardId branch.

---

## Explicit Wave 1A non-goals

```text
targeted exhaust
bulk exhaust
selection
Burning Pact full card
Shockwave / multi-enemy
Feel No Pain
Dark Embrace
Sentinel
Card Trigger Source Expansion
generic authored Continuation
Ethereal
universal zone-event bus
CFV redesign
ordinary Upgrade redesign
Phase 8 implementation
```

Reactive Powers and Card-owned trigger sources remain separate concepts:

```text
Feel No Pain / Dark Embrace
→ ongoing Power source reacts to any CardExhausted event

Sentinel
→ exact CardInstance acts as a Card trigger source
→ future Card Trigger Source Expansion
```

Do not merge these mechanisms.

---

## Required Wave 1A gates

Focused Automation source has been added under:

```text
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact.*
SlayTheSpireDemo.CardExpansion.Wave1A.GainEnergyCardEffect
```

Required checks remain:

```text
successful Exhaust commit → exactly one CardExhausted event
exact exhausted runtime card identity preserved
event payload fields match the committed zone facts
commit occurs before dispatch
committed CardZoneChanged record precedes event observation when writer is available
Discard destination → zero CardExhausted
Removed destination → zero CardExhausted
rejected replay → no duplicate event
zero-listener CardExhausted is valid
GainEnergy adapter resolves Base/Upgraded authored amount correctly
GainEnergy preview argument matches the same effective amount
```

Because C++ changed, required before seal:

```text
SlayTheSpireDemoEditor Win64 Development Build PASS
focused Wave 1A Automation PASS
```

Production acceptance still requires:

```text
Seeing Red production asset authored
→ Gain Energy resolves normally
→ self-exhaust commits normally
→ existing Exhaust presentation remains correct
→ focused event evidence confirms exactly one CardExhausted
→ no resolution fault
→ focused PIE PASS
```

Do not rerun unrelated sealed CFV or Upgrade test suites unless Wave 1A actually changes those contracts.

---

## Revised Wave 1 path

```text
Wave 1A — Exhaust Fact Surface
→ IMPLEMENTATION IN PROGRESS / VALIDATION PENDING

Wave 1B — Targeted Exhaust Primitive
→ FUTURE / NOT AUTHORIZED

Wave 1C — Selection + Targeted Exhaust Composition
→ FUTURE / NOT AUTHORIZED

Wave 1D — Reactive Exhaust Powers
→ FUTURE / NOT AUTHORIZED

Card Trigger Source Expansion
→ FUTURE INDEPENDENT FOUNDATION / NOT AUTHORIZED

Shockwave full card
→ DEFERRED TO MULTI-ENEMY CAPABILITY
```

---

## Current stop point

```text
Production Card Expansion
→ ACTIVE

Wave 1A design
→ LOCKED

Wave 1A C++ source
→ IMPLEMENTATION IN PROGRESS

Wave 1A Build / Automation / production asset / PIE
→ PENDING
```

Continue only with Wave 1A validation and Seeing Red production authoring. Do not start Wave 1B/1C/1D, Card Trigger Source Expansion, multi-enemy or Phase 8 as part of this slice.
