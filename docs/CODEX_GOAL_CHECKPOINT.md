# Codex Goal Checkpoint — Production Card Expansion

Last updated: **2026-09-07**

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

Wave 1C — Selection Primitive / First Consumer Closure:
IMPLEMENTATION UPDATED / BUILD + AUTOMATION + NATIVE HUD PIE REVALIDATION REQUIRED
```

Wave 1A validation completion was explicitly confirmed by the user on 2026-09-06. The validated production baseline includes `DA_Card_SeeingRed`.

## Current branch

```text
Wave-1C
```

Wave 1B merge commit:

```text
03d7941746e91b065fe247a2d18d5969da03872f
```

The former Wave 1B development branch was:

```text
card-expansion-wave1b-targeted-exhaust
```

Wave 1B source was merged into `main`, validated and sealed. Do not continue implementation on the old branch.

Wave 1C is currently under revalidation after the 2026-09-07 playable-selection/cancel/E2E closure changes. Do not mark it sealed from the earlier 7/7 result.

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

Wave 1B authority / execution record:

```text
docs/CardExpansionWave1BTargetedExhaustPrimitive.md
docs/CardExpansionWave1BExecution.md
```

Current Wave 1C authority / execution records:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
docs/CardExpansionWave1CSelectionExecution.md
docs/CardExpansionWave1CSelectExhaustExecution.md
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

Do not reopen Wave 1A during Wave 1C validation unless a concrete failure directly implicates the sealed contract.

---

## Wave 1B source on main

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

`UExhaustCardAction` retains the exact typed `FCardZoneMutationResult` for future authored composition. Wave 1B does not own selection semantics.

Locked Wave 1B boundaries remain:

```text
no Selection
no Burning Pact full card
no True Grit selection behavior
no bulk Exhaust
no AnyZone universal movement API
no Feel No Pain / Dark Embrace
no Sentinel / Card Trigger Source Expansion
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

## Wave 1C current implementation

### Selection primitive

```text
FSelectionRequest / FSelectionResult
USelectionResolver
USelectionRequestAction hold/resume lifecycle
UAuthoredContinuation
```

Selection state remains Gameplay-owned.

### Request-level cancel policy

```text
ESelectionCancelPolicy::Allowed
→ generic legal cancel path

ESelectionCancelPolicy::Forbidden
→ mandatory request cannot release the waiting Action through cancel
```

`USelectExhaustHandCardEffect` uses `Forbidden`, preventing Burning Pact from skipping its exhaust step and continuing into the already-authored Draw Effect.

### Native pending-card input bridge

```text
Native Hand card click
→ UBattleHUDWidget::SelectCard(RuntimeId)
→ UBattleHUDViewModel pending-selection helper
→ BattleSelectionRequest Gameplay facade
→ USelectionResolver::SubmitResult
```

The bridge is RuntimeId-only. UI does not receive authoritative candidate pointers and does not touch the ActionQueue. Ordinary busy-resolution input remains unchanged when no pending selection exists.

### Burning Pact transient Automation shape

```text
Cost 1
Effects = [SelectExhaust, Draw]
Base Draw 2
Upgraded Draw 3
DefaultDestination = Discard
```

The test fixture is transient C++ content. It intentionally does not use or validate a repository binary `DA_Card_BurningPact` asset.

### Owner-authored binary test content

The `Wave-1C` branch may contain user-authored:

```text
Content/.../DA_Card_BurningPact.uasset
Content/.../Maps/L_BattleTest.umap
```

These files are ad-hoc test content owned by the user. The 2026-09-07 closure change does not inspect, modify, or treat them as production acceptance evidence. Production asset acceptance remains separate.

---

## Current Wave 1C validation inventory

Expected focused Automation inventory after the closure changes:

```text
5 Selection primitive cases
6 SelectExhaust / Burning Pact cases
= 11 total Wave 1C cases
```

New cases cover:

```text
mandatory cancel rejection
base exhaust -> draw 2
upgraded exhaust -> draw 3
Effects order
FinishCardPlay completion
Native HUD C++ pending-card click route
```

The earlier `7/7 PASS` result predates these changes. It remains historical evidence only and does not validate the current branch head.

Required current gates:

```text
[ ] SlayTheSpireDemoEditor Win64 Development Build
[ ] SlayTheSpireDemo.CardExpansion.Wave1C focused Automation (expected 11 cases)
[ ] production Native HUD PIE selection smoke
[ ] user seal confirmation after the above pass
```

---

## Stop point

```text
Wave 1A
→ COMPLETE / VALIDATED / SEALED

Wave 1B
→ COMPLETE / VALIDATED / SEALED

Wave 1C-A / 1C-B current branch
→ IMPLEMENTATION UPDATED
→ REVALIDATION REQUIRED
→ NOT SEALED
```

Next action is not more feature expansion. First re-run the current Wave 1C build/Automation/Native HUD PIE gates. Only after those pass should Wave 1C be marked validated/sealed or production card content be accepted.
