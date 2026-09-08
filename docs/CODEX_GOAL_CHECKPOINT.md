# Codex Goal Checkpoint — Production Card Expansion

Last updated: **2026-09-08**

## Latest user-requested side refactor — Automatic Card Descriptions

HEAD verified: `5128e2f936416eb16fc8e4f9d7d45272a66368f7`. This refactor is uncommitted in the working tree; existing staged Interior/environment work and user-edited BurningPact / untracked TrueGrit remain preserved. No card assets or maps were saved by this refactor; five Status DisplayName assets were localized through UE Python.

Implemented: default Effect-ordered localized Chinese descriptions, per-effect argument isolation, upgraded values/hit counts/selection modes, automatic Exhaust, explicit custom-template compatibility, focused tests and documentation. Durable scope and acceptance: `docs/AutomaticCardDescriptions.md`.

Completed validation: standard project generation; Development Editor Win64 build PASS; focused Automation 18/18 PASS; after status-name asset edits only ExistingAssets rerun 1/1 PASS. Reports and exact prefixes are recorded in `docs/Validation.md`. Do not rerun these passing gates without invalidation.

Next action / remaining blocker: USER ACTION REQUIRED, one visual Native `L_BattleTest` pass confirming Chinese multi-line descriptions fit and hover-preview styling remains readable. Await user observation before marking this refactor visually accepted. No further phase work is implied. Earlier card-expansion checkpoint below remains historical navigation for that initiative.

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
COMPLETE / VALIDATED / SEALED / MERGED TO MAIN

Wave 1C-C0 — Select-Exhaust Generalization:
DESIGN LOCKED / IMPLEMENTATION AUTHORIZED / ACTIVE ON MAIN / NOT SEALED

Wave 1C-C1 — True Grit Consumer:
NEXT AFTER C0 SEAL / NOT STARTED
```

Wave 1A validation completion was explicitly confirmed by the user on 2026-09-06. Wave 1B was subsequently validated and sealed. Wave 1C current-head revalidation was explicitly confirmed by the user on 2026-09-07 after the playable-selection, mandatory-cancel and committed-Presentation closure fixes.

Wave 1C-C0 design was explicitly locked and implementation on `main` was authorized by the user on 2026-09-08. C0 generalizes the existing Select-Exhaust effect before True Grit is authored.

---

## Current branch

```text
main
```

Wave 1C merge:

```text
PR #16
Merge commit: a9f26ee4bcc8f12a03ba10d5121eb0ff6ef8d523
```

Validated Wave 1C implementation head before the final documentation-only commits:

```text
64a405a414c009d508ec352bb200a47775016578
fix(wave1c): fade exhausted hand card in place
```

Final Wave 1C documentation head merged by PR #16:

```text
b29a7c794d75546957b443e039329225c4a853d8
docs(wave1c): record final seal checkpoint
```

Wave 1B merge commit:

```text
03d7941746e91b065fe247a2d18d5969da03872f
```

The former Wave 1B development branch was:

```text
card-expansion-wave1b-targeted-exhaust
```

Wave 1B and Wave 1C source are now on `main`; do not continue implementation on the old development branches.

Wave 1C-C0 is intentionally being developed directly on `main` per explicit user authorization. No Wave 1C-C0 feature branch is required.

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

Wave 1C authority / execution records:

```text
docs/CardExpansionWave1CSelectionPrimitive.md
docs/CardExpansionWave1CSelectionExecution.md
docs/CardExpansionWave1CSelectExhaustExecution.md
```

Current Wave 1C-C0 dedicated authority:

```text
docs/CardExpansionWave1CC0SelectExhaustGeneralization.md
```

This C0 authority is design-locked and implementation-authorized. True Grit production CardData is intentionally deferred to C1 after C0 is sealed.

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

Validated production card baseline:

```text
Seeing Red / 盛怒
BaseCost       = 1
UpgradedCost   = 0
BaseAmount     = 2
UpgradedAmount = 2
DefaultDestination = Exhaust
```

Do not reopen Wave 1A without a concrete regression that directly implicates its sealed contract.

---

## Wave 1B sealed result

Implemented narrow capability:

```text
exact UCardInstance currently in Hand
→ UExhaustCardAction
→ UDeckRuntime::TryExhaustHandCardCommit
→ Hand → ExhaustPile authoritative commit
→ exact FCardZoneMutationResult
→ committed CardZoneChanged record when available
→ FCardExhaustedEvent
→ Dispatcher
```

Wave 1B remains orthogonal to selection semantics.

Locked boundaries include:

```text
no Selection
no Burning Pact full card
no True Grit selection behavior
no bulk Exhaust
no AnyZone universal movement API
no Feel No Pain / Dark Embrace
no Sentinel / Card Trigger Source Expansion
```

Wave 1B is complete, validated and sealed.

---

## Wave 1C sealed implementation

### Selection primitive

```text
FSelectionRequest / FSelectionResult
USelectionResolver
USelectionRequestAction hold/resume lifecycle
UAuthoredContinuation
ESelectionCancelPolicy
```

Selection state remains Gameplay-owned.

### Cancel policy

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

The bridge is RuntimeId-only. UI does not receive authoritative candidate pointers and does not touch the ActionQueue. Ordinary busy-resolution input remains unchanged when no supported pending selection exists.

### Continuation Presentation writer propagation

Selection-generated dependent Actions inherit the current committed-Presentation writer at the common `USelectionRequestAction` boundary.

This ensures the selected-card exhaust record is committed in the same resolution before later Draw records.

### Burning Pact composition

Transient Automation authority:

```text
Cost 1
Effects = [SelectExhaust, Draw]
Base Draw 2
Upgraded Draw 3
DefaultDestination = Discard
```

The reusable composition contract is independent of a binary card asset.

### Committed Presentation order

Sealed base-resolution order:

```text
CardPlayed
→ Hand → ExhaustPile
→ DrawPile → Hand
→ DrawPile → Hand
→ PlayArea → DiscardPile
```

`UBattlePresentationController` reduces the exact Hand-exhaust record before later Draw records, preserving historical Hand indices.

### Native Hand-exhaust visual

The selected card exhaust visual is intentionally an in-place fade:

```text
translation 0 → 0
scale       1 → 1
opacity     1 → 0
```

After the record completes, the Controller formally removes the exact card from Hand and Draw presentation continues. This replaced the earlier movement-to-Exhaust visual.

---

## Wave 1C final Automation inventory

Prefix:

```text
SlayTheSpireDemo.CardExpansion.Wave1C
```

Inventory:

```text
5 Selection primitive cases
6 SelectExhaust / Burning Pact Gameplay + input cases
2 committed-Presentation regression cases
= 13 total cases
```

The Presentation regressions are:

```text
SlayTheSpireDemo.CardExpansion.Wave1C.BurningPact.PresentationRecordOrder
SlayTheSpireDemo.CardExpansion.Wave1C.Presentation.HandExhaustReducer
```

Coverage includes:

```text
allowed cancel behavior
mandatory cancel rejection
exact selected-card exhaust
base exhaust → draw 2
upgraded exhaust → draw 3
Effects order
FinishCardPlay completion
Native HUD pending-card click route
committed CardPlayed → Exhaust → Draw → Draw → Finish ordering
exact Hand→Exhaust reducer behavior and stale-index rejection
```

---

## Wave 1C final validation evidence

User-confirmed local validation on **2026-09-07**:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.CardExpansion.Wave1C focused Automation PASS (13/13)
[x] Native HUD PIE base Burning Pact PASS
[x] Native HUD PIE Burning Pact+ PASS
[x] mandatory selection cannot be cancelled/skipped into Draw PASS
[x] exhausted selected card fades in place PASS
[x] Draw 2 / Draw 3 animation playback PASS
[x] exhausted card does not reappear at the end of Hand PASS
[x] HUD returns to normal interaction after resolution PASS
[x] final user validation confirmation received
```

The former 5/5 and 7/7 results remain historical pre-closure evidence only. The current Wave 1C seal is based on the revalidated implementation above.

---

## Wave 1C-C0 locked implementation target

C0 generalizes the existing `USelectExhaustHandCardEffect` without renaming its UCLASS.

Blueprint-authored effective contract:

```text
BaseSelectionMode
BaseSelectionCount
UpgradedSelectionMode
UpgradedSelectionCount

Mode = Player | Random
Count = exactly N, authored >= 0
```

Core locked behavior:

```text
Player
→ Native HUD exact-N unique RuntimeId selection
→ auto-submit when N reached

Random
→ no pending UI
→ deterministic battle RNG
→ choose N unique candidates without replacement

both
→ canonicalize selected set to candidate order
→ same FSelectionResult / authored Continuation
→ UExhaustCardAction × N
```

Additional correctness requirements:

```text
- Resolver rejects duplicate selected objects
- UI remains RuntimeId-only
- count > candidates clamps to all candidates
- count 0 / no candidates is a no-op
- each real Exhaust remains an independent commit/event
- existing in-place fade Presentation remains unchanged
- existing Burning Pact defaults remain Player / 1 for Base and Upgrade
```

Full authority and seal gates:

```text
docs/CardExpansionWave1CC0SelectExhaustGeneralization.md
```

---

## Repository content merged with Wave 1C

PR #16 merged the user-authored binary content that was present on the Wave 1C branch:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```

They are now repository content on `main`. They were not modified by the C++ closure fixes, and they do not replace the reusable C++/Automation/PIE contracts as architecture authority.

---

## Stop point / next active slice

```text
Wave 1A
→ COMPLETE / VALIDATED / SEALED

Wave 1B
→ COMPLETE / VALIDATED / SEALED

Wave 1C-A / 1C-B
→ COMPLETE / VALIDATED / SEALED
→ MERGED TO MAIN (PR #16 / a9f26ee4bcc8f12a03ba10d5121eb0ff6ef8d523)

Wave 1C-C0
→ DESIGN LOCKED
→ IMPLEMENTATION AUTHORIZED
→ ACTIVE DIRECTLY ON main
→ NOT SEALED

Wave 1C-C1 / True Grit
→ WAITING FOR C0 SEAL
→ NOT STARTED
```

The next implementation work is C0 only. Do not author production True Grit CardData until C0 passes its Build, focused Automation, existing Wave 1C 13/13 regression, Player multi-select PIE, Random multi-select PIE and Burning Pact regression PIE gates.

Wave 1D Reactive Exhaust Powers, Card Trigger Source Expansion, multi-enemy work and Phase 8 remain separate future slices and are not implicitly authorized by the C0 design lock.
