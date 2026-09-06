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
COMPLETE / VALIDATED / SEALED

Wave 1B — Targeted Exhaust Primitive:
COMPLETE / VALIDATED / SEALED

Wave 1C — Selection Primitive:
WAVE 1C-A COMPLETE / VALIDATED / READY FOR SEAL; 1C-B COMPLETE / VALIDATED / READY FOR SEAL
```

Wave 1A validation completion was explicitly confirmed by the user on 2026-09-06. The validated production baseline now includes `DA_Card_SeeingRed`.

## Current branch

```text
main
```

Wave 1B merge commit:

```text
03d7941746e91b065fe247a2d18d5969da03872f
```

The former development branch was:

```text
card-expansion-wave1b-targeted-exhaust
```

Wave 1B source was merged into `main`, validated and sealed. Do not continue implementation on the old branch.

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

Wave 1A authority / execution record:

```text
docs/CardExpansionWave1AExhaustFactSurface.md
docs/CardExpansionWave1AExecution.md
```

Current Wave 1B authority / execution record:

```text
docs/CardExpansionWave1BTargetedExhaustPrimitive.md
docs/CardExpansionWave1BExecution.md
```

Current Wave 1C authority / execution record:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
docs/CardExpansionWave1CSelectionExecution.md
```

Future Card trigger-source design remains independently unauthorized:

```text
docs/CardTriggerSourceExpansionDesign.md
```

---

## Wave 1A sealed result

Wave 1A established:

```text
self-exhaust authoritative commit
→ exact FCardExhaustedEvent
→ BattleEventDispatcher
```

and added the narrow authored adapter:

```text
UGainEnergyCardEffect
→ existing UGainEnergyAction
```

Production validation card:

```text
Seeing Red / 盛怒
BaseCost       = 1
UpgradedCost   = 0
BaseAmount     = 2
UpgradedAmount = 2
DefaultDestination = Exhaust
```

Do not reopen Wave 1A during Wave 1B validation unless a concrete Wave 1B failure directly implicates the sealed contract.

---

## Wave 1B source now on main

Implemented narrow capability:

```text
exact UCardInstance currently in Hand
→ UExhaustCardAction
→ UDeckRuntime::TryExhaustHandCardCommit
→ Hand → ExhaustPile authoritative commit
→ exact FCardZoneMutationResult
→ committed CardZoneChanged record when available
→ same FCardExhaustedEvent rule
→ Dispatcher
```

`UExhaustCardAction` retains the exact typed `FCardZoneMutationResult` for future authored composition. Wave 1B does not implement generic Continuation.

Locked boundaries remain:

```text
no Selection
no Burning Pact full card
no True Grit selection behavior
no bulk Exhaust
no AnyZone universal movement API
no Feel No Pain / Dark Embrace
no Sentinel / Card Trigger Source Expansion
no Wave 1C
```

The negative wiring test explicitly declares its expected ResolutionFault log messages so the intended fault path is not misclassified as an Automation failure.

---

## Wave 1B sealed

Automated gates:

```text
SlayTheSpireDemoEditor Win64 Development Build PASS
SlayTheSpireDemo.CardExpansion.Wave1B.TargetedExhaust PASS
```

Focused suite covers:

```text
exact Hand target → ExhaustPile commit
non-target Hand card remains untouched
typed CommitResult preserves exact identity and zone facts
CardExhausted observes already-committed state
Presentation and Event agree with CommitResult
stale target → no duplicate commit/event
missing event wiring → ResolutionFault before commit
DeckRuntime mutation owner does not dispatch Event by itself
```

Wave 1B is complete, validated and sealed. Do not rerun sealed Wave 1A, CFV or Upgrade suites unless a concrete failure invalidates those contracts.

---

## Stop point

```text
Wave 1A
→ COMPLETE / VALIDATED / SEALED

Wave 1B
→ COMPLETE / VALIDATED / SEALED

Wave 1C-A
→ COMPLETE / VALIDATED / READY FOR SEAL

Wave 1C-B
→ COMPLETE / VALIDATED / READY FOR SEAL (Select-Exhaust consumer, Burning Pact shape)
```

Wave 1C-A Selection Primitive and Wave 1C-B Select-Exhaust consumer are implemented and validated (7/7 Automation PASS, no regression). Execution record: `docs/CardExpansionWave1CSelectExhaustExecution.md`. Next: user seal confirmation for 1C-A and 1C-B, then production `DA_Card_BurningPact` asset authoring in Unreal Editor (deferred to user).