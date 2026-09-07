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
COMPLETE / VALIDATED / SEALED
```

Wave 1A validation completion was explicitly confirmed by the user on 2026-09-06. Wave 1B was subsequently validated and sealed. Wave 1C current-head revalidation was explicitly confirmed by the user on 2026-09-07 after the playable-selection, mandatory-cancel and committed-Presentation closure fixes.

---

## Current branch

```text
Wave-1C
```

Current validated Wave 1C implementation head before these final documentation commits:

```text
64a405a414c009d508ec352bb200a47775016578
fix(wave1c): fade exhausted hand card in place
```

Wave 1B merge commit:

```text
03d7941746e91b065fe247a2d18d5969da03872f
```

The former Wave 1B development branch was:

```text
card-expansion-wave1b-targeted-exhaust
```

Wave 1B source is on `main`; do not continue implementation on the old branch.

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

The former 5/5 and 7/7 results remain historical pre-closure evidence only. The current Wave 1C seal is based on the revalidated current implementation above.

---

## Owner-authored binary test content

The `Wave-1C` branch contains user-authored:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```

Those files were not modified by the C++ closure fixes. The Wave 1C reusable behavior seal is established by the C++/Automation/PIE contract, not by treating these binary files as generic architecture authority.

Their inclusion in a merge to `main` remains a separate repository-content decision.

---

## Stop point

```text
Wave 1A
→ COMPLETE / VALIDATED / SEALED

Wave 1B
→ COMPLETE / VALIDATED / SEALED

Wave 1C-A / 1C-B
→ COMPLETE / VALIDATED / SEALED
```

Wave 1C no longer requires implementation or validation work before merge. The next repository operation is to decide whether the owner-authored `.uasset/.umap` changes should be included in `main`, then merge the validated Wave-1C branch accordingly.

Do not start Wave 1C-C or unrelated card expansion as part of the merge operation.