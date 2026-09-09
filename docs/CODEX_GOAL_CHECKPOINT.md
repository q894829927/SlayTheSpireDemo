# Codex Goal Checkpoint — Production Card Expansion

Last updated: **2026-09-09**

## Current resumable task — G4+G5 persistent selection ownership

HEAD verified: `963adbd27cc161a09ea1ff68c7e479719b8331cb`.
User explicitly authorized G5 after G4 visual acceptance failed; later confirmed
Editor closed for compilation. Dedicated scope: `docs/SelectionPresentationG5Execution.md`.

Completed, uncommitted: persistent GC-owned SelectionArea copies, hidden formal
Hand slots, exact same-object G4 transfer, stable Confirmed layout, transactional
Confirm, deferred synchronous snapshots/receipts, exact recorded/direct completion,
missing-correlation UI-only recovery, atomic Native surface sync before external
observers and removal of obsolete position-compensation compatibility code.
New runtime file: `UI/BattleHUDViewModelSelectionPresentation.cpp`; five G5 tests
in `CardSelectionInputRoutingTests.cpp`, supporting fixture classes updated.
No Gameplay rules/assets/G6 implementation changed. Prior user work preserved.

Validation: final standard project generation and Editor build PASS
(`Saved/Logs/G5RecoveryProjectFiles.log`, `Saved/Logs/G5RecoveryBuild.log`).
Initial foundation/selection run 39/39 PASS. Expanded validation first hit a test
fixture pure-virtual omission, then caught one runtime recovery-state overwrite;
both corrected. Final affected G5 + G0.FormalSlotOwnership run 6/6 PASS, no
warnings/failures/notRun, process exit 0 (`Saved/AutomationReports/G5Recovery/index.json`).
Other retained passing scope and intermediate failures: `docs/Validation.md`.
Do not add overlapping counts. No manual visual PASS is claimed.

Next action / USER ACTION REQUIRED: reopen Native `L_BattleTest`, select/deselect/
reselect; Confirm multiple Exhaust candidates and verify second/third stay in place
until their own animation, with no first-card flashback; Warcry selection transfers
the exact visible card to DrawPile. Check no duplicate/ghost/clipping/stuck input.
G4+G5 remains unsealed pending that evidence. Simultaneous disappearance belongs
to G6, which is not implemented or started here. Changes are not committed.

## Earlier resumable task — Selection Presentation G2

Selection Presentation G0 and G1 are complete, validated and sealed. G0's Native
`L_BattleTest` manual PIE acceptance was user-confirmed on 2026-09-09. G1 then
landed exact Selection-boundary outcome correlation plus writer-scoped optional
PresentationGroup metadata and canonical selected-identity manifests on `main`.
Durable evidence is recorded in `docs/SelectionPresentationG0Execution.md`,
`docs/SelectionPresentationG1Execution.md` and `docs/Validation.md`.

Current Selection Presentation status:

```text
G0: COMPLETE / VALIDATED / SEALED
G1: COMPLETE / VALIDATED / SEALED
```

User-confirmed G1 validation on 2026-09-09:

```text
[x] SlayTheSpireDemoEditor Win64 Development Build PASS
[x] SlayTheSpireDemo.SelectionPresentation.G1 prefix run PASS
[x] RecorderMetadata individual run PASS
[x] ManifestValidation individual run PASS
```

The prefix run plus the two individual invocations cover two distinct G1 tests.
No additional G0/G1 rerun is required unless a later edit invalidates passing
evidence or a concrete regression directly implicates those sealed contracts.

Next active development slice: **G2 — Controller semantic Group discovery / reducer
dry-run / interference**. G2 is semantic-only: it may inspect sealed immutable
Envelope facts, must not query concrete Widget/SelectionArea geometry or ownership,
and must keep visible Group playback disabled. Production SelectionArea ownership
remains deferred until the later G4/G5 migration prerequisites are satisfied.

## Earlier resumable task — Selection Presentation production repair

HEAD verified: `3abf80f0e2070ac798c164e8ab23522d4cad7dd9`. Changes are uncommitted. Contract and evidence: `docs/CardSelectionPresentationConstraints.md` section 16 and `docs/Validation.md`.

Completed: production Native HUD reparented/compiled/saved in UE; shared explicit Confirm routing now active; dimmed selection backdrop, centered formal selected cards, RuntimeId visual handoff to DrawPile, retained played-card hide/restore, and inherited generic Exhaust cleanup. Confirmed Hand cards now retain their selection-area render position through the handoff, so the existing generic Hand→Exhaust fade consumes the formal widget in place. Skip cleans both the moving and retained played-card visuals. No card/map/Legacy edits.

Validation performed: standard project generation and final Editor build PASS after the in-place Exhaust fix (`Saved/Logs/InPlaceExhaustFadeFinalBuild.log`). The final focused `SlayTheSpireDemo.CardSelection.Presentation` prefix passed 4/4, including the new `HandToExhaust.FadesInPlace` contract, with no warnings (`Saved/AutomationReports/CardSelectionPresentationInPlaceFinal/index.json`). Earlier explicit-Confirm evidence remains in `Saved/AutomationReports/SelectionBoundaryConfirm/index.json`.

The later G0 draw/selection manual PIE pass and G1 focused build/Automation validation were completed and user-confirmed on 2026-09-09. This historical repair entry is retained for navigation; current forward work is G2.

## Earlier resumable task — Unified Card Selection Refactor

HEAD verified: `6fce24e39c53178b59561be31932659a0a542087`. Dedicated contract/status: `docs/CardSelectionRefactorConstraints.md`. Runtime and focused tests are implemented in the working tree; no commit created by the agent.

Completed: shared Execute-time current-Hand CandidateSource, Player/Random deferred pipeline, explicit boundary injection, compatibility-only old Hand Action, typed resolver failure dispositions, decision revision/direct-mode frozen publication and UI partial-selection clearing. New tests include multi-boundary cleanup, identical choices, source/continuation/insertion failures, no-history and PresentationUnavailable separation. Existing asset/map user edits remain untouched.

Validation performed: project generation and Development Editor Build PASS after user closed Live Coding. Seven specified prefixes: 43 tests, 42 passed, one historical single-envelope assertion failed; unified selection 12/12 PASS. Updated only that old test's split-envelope expectation, rebuilt PASS, reran only MultiExhaustRecordOrder 1/1 PASS. Logs/reports: `Saved/Logs/UnifiedSelectionBuild.log`, `Saved/Logs/UnifiedSelectionFinalBuild.log`, `Saved/AutomationReports/UnifiedSelection/index.json`, `Saved/AutomationReports/UnifiedSelectionRecordOrder/index.json`. `git diff --check` PASS.

The G0-focused manual draw/selection acceptance was completed on 2026-09-09. Broader historical A–D cases in the unified-selection design remain historical scope and do not block the sealed G0/G1 contracts unless explicitly reopened.

## Previous checkpoint context (historical; current task above takes precedence)

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

Selection Presentation G0:
COMPLETE / VALIDATED / SEALED

Selection Presentation G1:
COMPLETE / VALIDATED / SEALED

Selection Presentation G2:
NEXT ACTIVE SLICE / NOT STARTED

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

Selection Presentation G0 manual acceptance and G1 Editor Build/focused Automation validation were explicitly confirmed by the user on 2026-09-09. G0 and G1 are sealed; G2 is now the next active Selection Presentation slice.

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
Selection Presentation G0
→ COMPLETE / VALIDATED / SEALED

Selection Presentation G1
→ COMPLETE / VALIDATED / SEALED
→ exact outcome correlation + writer-scoped optional Group metadata

Selection Presentation G2
→ NEXT ACTIVE SLICE
→ Controller semantic Group discovery / reducer dry-run / interference
→ NOT STARTED

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

For Selection Presentation, resume from G2. Keep Group visible playback disabled and do not activate production SelectionArea ownership before the generic SingleRecord transition path and exact completion/recovery prerequisites are in place.

For the older Card Expansion track, C0 remains the recorded next implementation slice in that historical initiative. Do not author production True Grit CardData until C0 passes its Build, focused Automation, existing Wave 1C 13/13 regression, Player multi-select PIE, Random multi-select PIE and Burning Pact regression PIE gates.

Wave 1D Reactive Exhaust Powers, Card Trigger Source Expansion, multi-enemy work and Phase 8 remain separate future slices and are not implicitly authorized by the Selection Presentation G0/G1 seals.
