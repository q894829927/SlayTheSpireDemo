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
DESIGN LOCKED / NEXT ACTIVE SLICE / IMPLEMENTATION NOT STARTED
```

## Current branch

```text
main
```

Wave 1A planning/documentation was merged into `main`. Current implementation work must continue directly on `main` unless the user explicitly changes the workflow.

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

Current next-active dedicated authority:

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

Current production CardData set:

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

These six are existing baseline content and must not be recreated as part of Wave 1A.

Ordinary upgrade support for current typed Damage / Block / Draw / ApplyStatus effect fields is already sealed. Do not reopen the old Foundation-0 upgrade design.

---

## Exhaust capability split

Do not use “Exhaust is implemented” without qualification.

### Existing: self-exhaust after play

```text
UCardData::DefaultDestination = Exhaust
→ normal play lifecycle
→ FinishCardPlay
→ PlayArea → ExhaustPile authoritative commit
```

The zone mutation already exists.

### Missing: exact CardExhausted gameplay fact

Current `BattleEvent` surface does not yet expose `CardExhausted`.

Wave 1A adds only:

```text
successful self-exhaust commit
→ exact immutable FCardExhaustedEvent
→ Dispatcher
```

### Deferred: targeted exhaust

```text
select/specify exact CardInstance
→ explicit Exhaust mutation/action
```

Needed later by Burning Pact / Fiend Fire / Second Wind / related cards.

Targeted exhaust is not part of Wave 1A.

---

## Wave 1A locked producer boundary

`DeckRuntime` remains mutation owner only.

Required ordering:

```text
FinishCardPlay / current card-play composition boundary
→ request zone move commit
→ receive exact typed commit result
→ if committed && ToZone == ExhaustPile
   → build FCardExhaustedEvent from held Card + committed result
   → dispatch
```

Forbidden:

```text
DeckRuntime directly dispatches BattleEvent
DefaultDestination == Exhaust → assume commit success
dispatch before mutation
CardId / DisplayName special case
```

The event must describe a committed fact, not play intent.

---

## Wave 1A production validation card

Default validation card:

```text
Seeing Red
```

Reason:

```text
Gain Energy
+ self Exhaust
```

This keeps the validation card independent of selection, targeted exhaust, multi-enemy, reactive Power and Card Trigger Source Expansion.

`UGainEnergyAction` already exists; Wave 1A adds the narrow authored `UGainEnergyCardEffect` adapter required to construct it from typed Base/Upgraded values. Do not special-case Seeing Red in PlayCardAction or Energy owner code.

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

Do not merge these two mechanisms.

---

## Required Wave 1A gates

Focused Automation target:

```text
SlayTheSpireDemo.CardExpansion.Wave1A.ExhaustFact
```

Minimum required checks:

```text
successful Exhaust commit → exactly one CardExhausted event
exact exhausted runtime card identity preserved
event payload fields match the exact committed mutation result
commit occurs before dispatch
Discard destination → zero CardExhausted
Removed destination → zero CardExhausted
failed/rejected move → zero CardExhausted
no duplicate event
zero-listener CardExhausted is valid
GainEnergy adapter resolves Base/Upgraded authored amount correctly
GainEnergy preview argument matches the same effective amount
```

If C++ changes, required:

```text
SlayTheSpireDemoEditor Win64 Development Build PASS
```

Production acceptance:

```text
Seeing Red authored
→ Gain Energy resolves normally
→ self-exhaust commits normally
→ existing Exhaust presentation remains correct
→ focused event evidence confirms exactly one CardExhausted
→ no resolution fault
```

Do not rerun unrelated sealed CFV or Upgrade test suites unless Wave 1A actually changes those contracts.

---

## Revised Wave 1 path

```text
Wave 1A — Exhaust Fact Surface
→ NEXT ACTIVE SLICE

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

Wave 1A implementation
→ NOT STARTED
```

The next implementation work must follow:

```text
docs/CardExpansionWave1AExhaustFactSurface.md
```

Do not start Wave 1B/1C/1D, Card Trigger Source Expansion, multi-enemy or Phase 8 as part of Wave 1A.