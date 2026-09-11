# Validation

This document records trusted historical validation evidence and the rules for making new validation claims.

## Shipping packaging fix and Native HUD alignment — 2026-09-10

`InteriorDayNightController.cpp` now uses the runtime-safe directional light component
API instead of the editor-only `ADirectionalLight::GetComponent()` accessor. Through
Unreal MCP, the Native `WBP_BattleHUD_Native` layout was also corrected so the enemy
presentation and character image share the player's vertical baseline; moving
`EnemyPanel` with them keeps the enemy HP and status containers parallel to the player.
The existing left/right placement and enemy presentation sizes were preserved.

- Bundled UE 5.8 project-file generation: **PASS**.
- Win64 Shipping target build: **PASS**; `SlayTheSpireDemo-Win64-Shipping.exe` linked.
- Focused `SlayTheSpireDemo.Interior.DayNight` Automation: **1/1 PASS**, 0 warnings,
  failures or notRun (`Saved/AutomationReports/InteriorDayNightPackagingFix/index.json`).
- MCP UMG compile/save: **PASS**; `WBP_BattleHUD_Native` compiled and saved.
- MCP PIE visual check: **PASS**; player and enemy HP bars were visibly on the same
  horizontal line after the layout change.
- Full Win64 Shipping `BuildCookRun` with Cook/Stage/Pak/Archive: **PASS**, exit 0;
  archived at `Saved/Packaged/InteriorDayNightFix_WithHUDAlignment`.
- The earlier standalone Development Editor build attempt was blocked before compile
  by active Live Coding; the subsequent UAT combined Editor/Shipping build completed
  successfully. No manual packaged-game acceptance gate is claimed.

## Ruined Citadel Battle Background — 2026-09-10

New `L_Battle_RuinedCitadel`, imported image, presentation-only background actor/widget
and both default map settings were created/configured through Unreal MCP.
Widget/actor compilation, save/load, Native Presenter references, settings readback
and focused floating PIE background/End Turn checks: **PASS**. The next turn displayed
74/80 HP and 5/5 energy. Runtime/cleanup log queries found no Blueprint runtime error,
Accessed None or ResolutionFault. Scope and evidence paths are recorded in
[Ruined Citadel battle level](RuinedCitadelBattleLevel.md). No C++ build, Automation
or packaged-game claim; unrelated pending visual gates remain unchanged. A follow-up
MCP cleanup removed template lights/fog/floor/PlayerStart and disabled map AI and
precomputed lighting; the post-cleanup PIE log reached battle-ready state, while the
MCP transport disconnected before a second screenshot.

## Awakened One Monster Animation — 2026-09-10

The Native combatant Presentation widget now uses the saved Awakened One `Idle_2`, `Hit` and
`Attack_1` texture sequences. The first static-looking result was traced to unsaved imported
textures and the near-static `Idle_1` source; the active profile now points at the visible
`Idle_2` sequence. Enemy attack presentation is mapped from committed Attack damage aimed at
the player. Scope and the exact manual gate are documented in
[Awakened One monster animation](AwakenedOneCharacterAnimation.md).

- Bundled UE 5.8 project-file generation: **PASS** (`Saved/Logs/AwakenedOneMonsterAnimationFinalProjectFiles.log`).
- Development Editor Win64 build: **PASS** (`Saved/Logs/AwakenedOneMonsterAnimationFinalBuild.log`).
- MCP `WBP_CombatantPresentation` compile/save: **PASS**.
- Imported runtime assets: **48 Idle_2 + 8 Hit + 24 Attack_1** textures under
  `Content/SlayTheSpireDemo/UI/Textures/AwakenedOne`, with the static fallback retained.
- Focused floating PIE on `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`: **PASS for visible
  Idle_2 playback**; captures separated by 1.2 seconds showed different tail poses and eye state.

**USER ACTION REQUIRED:** manually end the turn in `L_Battle_RuinedCitadel` to observe the
enemy `Attack_1` pose during player damage, then play an Attack card to observe the enemy `Hit`
pose. No final attack/hit visual PASS is claimed until that interaction is observed.

## Enemy Presentation and Intent Cluster Layout — 2026-09-11

Through Unreal MCP, the Native HUD now keeps `Combatant_EnemyPresentation` and
`EnemyIntentPanel` under one `EnemyCombatantCluster` Overlay. The intent is centered relative to
the enemy presentation, and the enemy instance is scaled to `1.18` around its bottom center;
the shared combatant template and player instance remain at scale `1.0`.

- MCP `WBP_BattleHUD_Native` and `WBP_CombatantPresentation` compile/save: **PASS**.
- Focused floating PIE on `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`: **PASS**; the
  enemy is visibly larger, the player keeps its previous size, and the intent icon/damage value
  sits immediately above the enemy.
- Removed the unused Native-HUD `Img_PlayerCharacter` and `Img_EnemyCharacter` brushes through
  MCP; Designer and PIE now contain one character presentation per combatant with no overlap.
- No Gameplay, ViewModel or record-schema changes were made.

## Ironclad Character Animation — 2026-09-10

The Native combatant Presentation widget now plays the imported Ironclad profile from
`UI/images/characters/ironclad`: 120-frame looping Idle, 8-frame one-shot Hit, a reversible
Attack-card-only lunge, Victory pulse and corpse-based Defeat. The lunge keeps the authored Idle
frame cadence and resumes that source timeline when it ends, avoiding the previous attack pose
skip/reset. Record mapping, source limitations, Blueprint tuning parameters and the manual
acceptance checklist are documented in
[Ironclad character animation](IroncladCharacterAnimation.md).

- Bundled UE 5.8 project-file generation: **PASS** (`Saved/Logs/IroncladCharacterAnimationProjectFiles.log`).
- Development Editor Win64 build: **PASS** (`Saved/Logs/IroncladCharacterAnimationBuild.log`).
- Attack cadence correction project generation/build: **PASS** (`Saved/Logs/IroncladAttackCadenceFixProjectFiles.log`,
  `Saved/Logs/IroncladAttackCadenceFixBuild.log`). The build compiled
  `BattleHUDCombatantPresentationWidgetBase.cpp` successfully after the frame-timing fix.
- `CompileAllBlueprints`: **0 errors**, `WBP_CombatantPresentation` successful; the commandlet
  reported only the project's existing unrelated warnings (`Saved/Logs/IroncladCharacterAnimationBlueprints.log`).
- Imported runtime assets: **120 Idle + 8 Hit + 1 corpse** textures under
  `Content/SlayTheSpireDemo/UI/Textures/Ironclad`.

**USER ACTION REQUIRED:** run the Native `L_BattleTest_Native` PIE visual checklist in the
dedicated document. Headless build/import evidence does not prove Slate frame timing, the
Blueprint image binding or final visual alignment.

## Fan Hand / Attack Targeting — 2026-09-10

Implementation and complete evidence: [Hand interaction](HandFanTargetingInteraction.md).
Final standard UE 5.8 project generation/build PASS; final affected Automation
union **11/11 PASS**, no warnings/failures/notRun, process exit 0, report
`Saved/AutomationReports/HandInteractionVerified/index.json`. That document records
the exact scopes, retained G0/G6/R8/Preview evidence, intermediate aborted runs and
the stale R8 negative-case correction without adding overlapping totals.
Production Native asset installation, private arrow tint/input suppression and
Selection continuity using the fan host are covered. **USER ACTION REQUIRED:**
Native `L_BattleTest` visual/hover/targeting checklist. No PIE PASS is claimed.

The Native HUD preview boundary and the Presenter-level PlayerController input
component now support mouse-right cancellation across the local game viewport.
Standard project-file generation and the Development Editor Win64 build passed
after this follow-up (`Saved/Logs/RightClickGlobalFinalProjectFiles.log`,
`Saved/Logs/RightClickGlobalFinalBuild.log`). The focused regression
`SlayTheSpireDemo.CardSelection.Presentation.Input.RightMouseButtonCancel` passed
**1/1**, with 0 warnings, failures or notRun, process exit 0
(`Saved/AutomationReports/RightMouseButtonCancel/index.json`). It covers ordinary
Attack target selection and confirms that a mandatory Gameplay selection remains
pending when its cancel policy is `Forbidden`. The headless test calls the shared
cancel route directly, while the Presenter infrastructure regression confirms that
the global component is pushed onto the local PlayerController. A real mouse click
still belongs to the Native `L_BattleTest` PIE gate above.
The affected `CardSelection.Presentation.G5` + `.Input` regression union also passed
**8/8**, with 0 warnings/failures/notRun (`Saved/AutomationReports/RightClickCancelSelectionRegression/index.json`).

The incoming-card fan-layout follow-up also passed the standard project-file generation
and Development Editor build (`Saved/Logs/HandFanPredictionProjectFiles.log`,
`Saved/Logs/HandFanPredictionBuild.log`). The focused Native hand, asset/arrow policy and
`DrawToHandSequentialPresentation` regression set passed **4/4** with no warnings,
failures or notRun (`Saved/AutomationReports/HandFanPrediction/index.json`). The new
layout parameters are exposed on `WBP_BattleHUD_Native` under `Battle HUD|Hand Layout`;
actual Blueprint defaults and the no-vertical-frame draw motion still require the Native
`L_BattleTest` PIE check above.

## Selection Presentation G4+G5 migration — 2026-09-09

Historical implementation base HEAD `963adbd27cc161a09ea1ff68c7e479719b8331cb`.
User authorized G5 after the isolated G4 visual failure below. Scope and manual checklist:
`docs/SelectionPresentationG5Execution.md`. No assets or Gameplay rules changed.
The coherent G4+G5 implementation subsequently landed in:

```text
c7f1a799708dbce12712f4c418b004ac627d9b03
g4+g5完成
```

- Standard bundled UE 5.8 project generation + Development Editor build PASS
  after user closed Editor. Final evidence: `Saved/Logs/G5RecoveryProjectFiles.log`
  and `Saved/Logs/G5RecoveryBuild.log`, process exit 0.
- Initial Automation **39/39 PASS** (33 clean, 6 existing fixture warnings), no
  failures/notRun, process exit 0: `Saved/AutomationReports/G5Initial/index.json`.
  Exact union: `SlayTheSpireDemo.CardSelection.Presentation`,
  `SlayTheSpireDemo.SelectionPresentation.G0`, `.G1`, `.G3`,
  `SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection`, and
  `SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop`.
- Expanded run `Saved/Logs/G5FinalAutomation.log` aborted on a missing pure-virtual
  preview implementation in the new no-destination test Effect. No complete
  report exists. Before abort, exact G4.SelectionAreaExactVisualTransfer,
  G4.SelectionAreaPrepareRollback and G5.ConfirmRejectionAndDirectOutcome passed
  under the CardSelection.Presentation prefix. The fixture interfaces were fixed.
- Completion run **9 passed / 1 failed**, no warnings/notRun, process exit 0:
  `Saved/AutomationReports/G5Completion/index.json`. The JSON failure, despite
  successful process exit, exposed missing-correlation recovery being overwritten
  by a later resolving-state notification; runtime now preserves unavailable
  state through same-battle read/snapshot updates. Scope: four G5 tests other
  than ConfirmRejectionAndDirectOutcome, CardSelection.Presentation.HandTo*,
  CardSelection.Presentation.Input*, and SelectionPresentation.G0.FormalSlotOwnership.
- Final affected rerun **6/6 PASS**, no warnings/failures/notRun, process exit 0:
  `Saved/AutomationReports/G5Recovery/index.json`,
  `Saved/Logs/G5RecoveryAutomation.log`. Exact union:
  `SlayTheSpireDemo.CardSelection.Presentation.G5` (five tests) plus
  `SlayTheSpireDemo.SelectionPresentation.G0.FormalSlotOwnership`.

Coverage includes GC during ownership publication, selection/deselection,
observer coherence, same-object retention and stable waiting transforms across
three Draw/Exhaust children, decline/cancel, rejection/retry, battle/HUD replacement,
synchronous snapshots/receipts during submit, terminal display replacement,
recorded/direct zero-destination completion and missing-metadata UI-only recovery.
Passing unaffected evidence is retained; overlapping run counts are not added.
Headless synthetic geometry does not validate real Slate frames or Blueprint layout.

**MANUAL PIE GATE: PASS / USER CONFIRMED 2026-09-09.** The user exercised the
production Native `L_BattleTest` G4+G5 path and confirmed:

```text
[x] select / deselect / reselect behaves correctly
[x] multiple selected Exhaust candidates remain at their own displayed positions while waiting
[x] sequential consumption starts from each exact visible SelectionArea card
[x] Warcry transfers the exact visible selected card toward DrawPile
[x] no flashback
[x] no duplicate
[x] no ghost
[x] no clipping
[x] no stuck input
```

This closes the coherent G4+G5 production gate. G4+G5 are **COMPLETE / VALIDATED /
SEALED**. The earlier isolated G4 compatibility failure remains historical evidence
and is not retroactively declared passed. **G6 simultaneous N-child Group playback
is not implemented and is the next active Selection Presentation stage.**

## Sequential selection Exhaust drift repair — 2026-09-09

Base HEAD `963adbd27cc161a09ea1ff68c7e479719b8331cb`, uncommitted repair on top
of the compilation fix below. Scope/root cause and manual acceptance are in
`docs/SelectionPresentationG4Execution.md`, sequential Exhaust follow-up.
First attempt was blocked by Live Coding. After user closure, standard project
generation and build PASS (target already up-to-date), logs
`Saved/Logs/SelectionDriftProjectFiles.log`, `Saved/Logs/SelectionDriftBuild.log`.
First routing-only repair: **8/8 PASS**, one existing fixture warning, exit 0;
`Saved/AutomationReports/SelectionDrift/index.json`. User still reported drift;
this run did not cover waiting-card layout compensation.

After adding waiting Hand position compensation: standard UE 5.8 project
generation and Development Editor Win64 build PASS (exit 0); logs
`Saved/Logs/SelectionWaitingDriftProjectFiles.log` and
`Saved/Logs/SelectionWaitingDriftBuild.log`.
Unattended `-nullrhi` focused `SlayTheSpireDemo.CardSelection.Presentation`:
**9/9 PASS** (8 clean, 1 with the existing ProductionConfirmRouting partial-fixture
SelectionAreaHost warning), 0 failed, 0 notRun, process exit 0.
Evidence: `Saved/AutomationReports/SelectionWaitingDrift/index.json` and
`Saved/Logs/SelectionWaitingDriftAutomation.log`. `git diff --check` PASS.

**HISTORICAL G4-ONLY MANUAL PIE GATE: FAIL.** The second selected card still
briefly jumped to the first card's position before returning to its own. The nine
automated passes remain coordinate/protocol evidence only; the waiting-position
compatibility experiment did not establish visual continuity. This failure is
preserved because it explains why the coherent G4+G5 migration was authorized.

The later G5 production SelectionArea ownership path superseded this failed
compatibility route and passed its combined manual gate above. Therefore older
phrasing such as “G5 unstarted pending G4 acceptance” is historical only and MUST
NOT be interpreted as current project status. Same-tick safe-group fades remain G6.

## Card transition compilation fix — 2026-09-09

Base HEAD `963adbd27cc161a09ea1ff68c7e479719b8331cb`, uncommitted fix.
Added the explicit HorizontalBox include to `BattleHUDCardTransitionWidget.cpp`
and renamed two local Overlay slots to avoid hiding `UWidget::Slot`, resolving
C2664 and C4458 without changing playback behavior.

Standard bundled UE 5.8 project generation and Development Editor Win64 build
PASS (exit 0). Evidence: `Saved/Logs/CardTransitionCompileFixProjectFiles.log`
and `Saved/Logs/CardTransitionCompileFixBuild.log`.
Unattended UnrealEditor-Cmd with `-nullrhi` ran the focused
`SlayTheSpireDemo.CardSelection.Presentation` prefix: **7/7 PASS**, 0 failed,
0 notRun, exit 0 (6 clean passes, 1 pass with warning). ProductionConfirmRouting
reported the existing partial-fixture SelectionAreaHost creation warning;
both G4 visual-transfer/prepare-rollback tests passed without warnings.
Evidence: `Saved/AutomationReports/CardTransitionCompileFix/index.json` and
`Saved/Logs/CardTransitionCompileFixAutomation.log`. `git diff --check` PASS.
No Blueprint/assets or visual behavior changed; no manual PIE gate is required
for this compilation-only fix, and no new visual acceptance is claimed.

## G0 A/B/C review and draw adoption — 2026-09-09

Base HEAD `2a4687705f9a7b3d1cc240dcd4e7d08a0701abcb`, uncommitted review changes.
Scope and manual steps: `docs/SelectionPresentationG0Execution.md`. Fixed completed
draw Widgets remaining HitTestInvisible after RuntimeId-keyed formal adoption.
Preserved the pre-existing delegate-binding edit. No asset/map changes.

**AUTOMATED GATES:** standard bundled UE 5.8 project generation PASS. First build
attempt was blocked by active Live Coding; after the user closed the editor,
Development Editor Win64 build PASS (`Saved/Logs/G0ReviewBuild.log`). After a
test-assertion correction, project generation and build PASS again
(`Saved/Logs/G0ReviewProjectFiles.log`, `Saved/Logs/G0ReviewFinalBuild.log`).

UnrealEditor-Cmd ran unattended with `-nullrhi`,
`-TestExit="Automation Test Queue Empty"` and the following RunTests scope:

```text
SlayTheSpireDemo.SelectionPresentation.G0
+SlayTheSpireDemo.Phase6UIA2N.R8.Zone.DrawToHandSequentialPresentation
+SlayTheSpireDemo.CardSelection.Presentation
```

Initial report: 13 tests, 11 clean PASS, 1 PASS with warning, 1 FAIL, exit 0
(`Saved/AutomationReports/G0Review/index.json`, `Saved/Logs/G0ReviewAutomation.log`).
The G0 FormalSlotOwnership failure was an incorrect test expectation: UE defaults
UUserWidget to SelfHitTestInvisible, which permits child input. The corrected
assertion accepts it or Visible. After the test-only rebuild, reran only
`SlayTheSpireDemo.SelectionPresentation.G0.FormalSlotOwnership`: **1/1 PASS**, no
warnings, exit 0 (`Saved/AutomationReports/G0ReviewFormalSlot/index.json`,
`Saved/Logs/G0ReviewFormalSlot.log`). Other passing evidence remains valid.

The initial warning was ProductionConfirmRouting's intentionally partial test HUD
without a Canvas WidgetTree root: dormant SelectionAreaHost could not be created.
Its Confirm routing assertions passed; this fixture does not establish production
Host geometry/visual acceptance. G0's 8 tests now have passing evidence, including
DrawAdoption; R8 sequential draw and the four Selection Presentation tests passed.
`git diff --check` PASS.

**MANUAL PIE GATE: PASS / USER CONFIRMED 2026-09-09.** The user completed the
focused Native `/Game/SlayTheSpireDemo/Maps/L_BattleTest` draw/play and Warcry
candidate interaction required by the G0 execution record and confirmed:

```text
[x] newly drawn card remains clickable after Draw Presentation completes
[x] newly drawn affordable card can be played normally
[x] no duplicate Hand card / duplicate formal slot observed
[x] Warcry newly drawn card is available as a Selection candidate
[x] select / deselect / reselect / explicit Confirm flow works normally
[x] input is restored after the full Presentation / Selection flow completes
```

This closes the remaining G0 manual Gate. G0-A/B/C are **COMPLETE / VALIDATED /
SEALED**. This acceptance does not claim later G1+ PresentationGroup behavior or
production SelectionArea ownership migration.

## Selection Presentation production repair — 2026-09-08

Base HEAD `3abf80f0e2070ac798c164e8ab23522d4cad7dd9`. Production Native HUD reparented in UE to `BattleHUDSelectionWidget`, Blueprint compilation and targeted save succeeded. Existing card/map assets preserved. **AUTOMATED GATES:** bundled UE project generation and Editor builds PASS; final runtime build `Saved/Logs/SelectionPresentationFinalBuild.log`, subsequent test-only builds `SelectionPresentationTestBuild.log` and `SelectionBoundaryConfirmTestBuild.log`. Closed scope in `docs/CardSelectionPresentationConstraints.md` section 16 ran **12 tests: 11 PASS / 1 FAIL**, no warnings (`Saved/AutomationReports/SelectionPresentationRepair/index.json`). Production selection Confirm, transfer/skip cleanup, multi-select and generic Exhaust passed. The old Draw-before-selection test still assumed click-to-submit; updated to require a separate explicit Confirm, rebuilt only changed tests and reran that gate **1/1 PASS**, exit 0 (`Saved/AutomationReports/SelectionBoundaryConfirm/index.json`). Runtime unchanged after the first run; other passing gates not repeated.

**HISTORICAL MANUAL GATE NOTE:** this pre-redesign repair originally required one Native `L_BattleTest` Warcry sequence. It is no longer an active blocker: the later G0 and coherent G4+G5 production paths received explicit user PIE acceptance. Preserve this entry as historical evidence only; current Selection Presentation authority is the sealed G0-G5 execution chain.

## Selection Presentation in-place Exhaust follow-up — 2026-09-09

The confirmation handoff preserved a selected formal Hand card's render
translation and visibility. The existing generic Hand→Exhaust opacity fade used
that transform, so the card disappeared at its confirmed selection position
without a separate consume animation. Standard project generation and the final
Development Editor build passed (`Saved/Logs/InPlaceExhaustFadeFinalBuild.log`).
The focused `SlayTheSpireDemo.CardSelection.Presentation` prefix passed **4/4**,
including `HandToExhaust.FadesInPlace`, with no warnings
(`Saved/AutomationReports/CardSelectionPresentationInPlaceFinal/index.json`).

**HISTORICAL / SUPERSEDED VISUAL PATH:** this in-place formal-Hand compatibility
approach was later superseded by persistent SelectionArea ownership. The isolated
G4 compatibility path ultimately failed its multi-card visual gate; the coherent
G4+G5 replacement passed the final user PIE gate above. Do not use this entry as
current production ownership guidance.

## Interactive Draw / Selection Boundary Review — 2026-09-08

Subsequent unified-selection refactor (HEAD `6fce24e`, uncommitted runtime changes): standard project generation and Development Editor build PASS after user saved/closed Live Coding. New `SlayTheSpireDemo.CardSelection.Unified` tests: **12/12 PASS**. Full closed scope listed in `docs/CardSelectionRefactorConstraints.md` section 19: **43 tests, 42 passed (9 with expected rejection/degradation warnings), 1 failed**; evidence `Saved/AutomationReports/UnifiedSelection/index.json`. The one failure was C0 `MultiExhaustRecordOrder`'s historical single-envelope expectation. Updated that test to verify the intentional prefix/continuation split while retaining canonical exhaust/cleanup assertions, rebuilt successfully, and reran only that test: **1/1 PASS, 0 warnings**, exit code 0; evidence `Saved/AutomationReports/UnifiedSelectionRecordOrder/index.json`. Runtime code was unchanged after the first run; passing Gates were not repeated. Build logs: `Saved/Logs/UnifiedSelectionBuild.log`, `Saved/Logs/UnifiedSelectionFinalBuild.log`. The four focused ordering cases were originally a standalone manual gate for this refactor; later Selection Presentation execution supplied newer production visual evidence. This historical entry does not override the current G0-G5 seal.

Base HEAD `d9d12ec`. Fixed missing `Actions/BattleActionQueue.h` in the new interactive boundary test; C2027 and cascading C2661 resolved. **AUTOMATED GATES:** standard bundled UE 5.8 project generation and Development Editor Win64 build PASS; `SlayTheSpireDemo.CardExpansion.Wave1CC1.DrawPileTop` **7/7 PASS**, process/test exit code 0. Evidence: `Saved/Logs/SelectionBoundaryReview.log`. The original C1 review still required dedicated Warcry PIE before a standalone C1 seal. Later G4+G5 user PIE acceptance confirms the shared exact visible SelectionArea→DrawPile transfer and normal continued input, but no dedicated final C1 user-seal record exists. Therefore C1 is correctly described as **IMPLEMENTED / MERGED TO main VIA PR #18 / FINAL STANDALONE SEAL NOT RECORDED**, not “not started” and not fabricated as sealed. Runtime review findings are recorded in `docs/CardExpansionWave1CC1InteractiveSelectionBoundaryFix.md`.

## Automatic Card Descriptions — 2026-09-08

Follow-up Exhaust color change: card-final Exhaust RichDescription now uses the `Exhaust` style in `DT_BattleCardTextStyles`, sRGB `#EFC851` / RGB(239, 200, 81). UE tool save and readback confirmed linear RGBA approximately (0.863157, 0.577580, 0.082283, 1), with Default font/size preserved. Standard project generation and Development Editor build PASS; affected `SlayTheSpireDemo.Cards.AutomaticDescription.CompositionAndPreview` PASS (1 passed, 0 failed, 0 warnings). Report: `Saved/AutomationReports/ExhaustKeywordColor/index.json`. Plain descriptions remain markup-free. Visual PIE not performed; minimal user check is SeeingRed's final “消耗。” in Native `L_BattleTest`, expected gold with unchanged size.

User-requested refactor, detailed contract: `docs/AutomaticCardDescriptions.md`. Standard UE 5.8 project generation and Development Editor Win64 build PASS after user closed Live Coding and an explicit BattleManager include fixed independent GainEnergyCardEffect compilation. Final build log: `Saved/Logs/AutomaticCardDescriptionsBuild.log`.

Focused Automation: `Cards.AutomaticDescription`, `UIA3.RichCardTextBaseline`, `CardExpansion.Wave1CC0.Description`, `Phase6UIA3.DynamicText`, `UIA3.CardPlayedRichHandoff` (all prefixed `SlayTheSpireDemo.`): **18 passed, 0 failed, 0 warnings**. Evidence: `Saved/AutomationReports/AutomaticCardDescriptions/index.json`.

After UE-supported saving of five Chinese Status DisplayName assets, only the invalidated `SlayTheSpireDemo.Cards.AutomaticDescription.ExistingAssets` gate was rerun: **1 passed, 0 failed, 0 warnings**. This loaded eight existing production cards, base and upgraded, and confirmed generated Chinese text. Evidence: `Saved/AutomationReports/AutomaticCardDescriptionAssetsChinese/index.json`; save log: `Saved/Logs/LocalizeCardStatusNames.log`. No subsequent C++ changes.

**MANUAL PIE — USER ACTION REQUIRED:** one Native `L_BattleTest` card-face readability pass for multi-line text, Exhaust line and hover-preview coloring, as specified in the dedicated document. No PIE or packaged-game acceptance is claimed.

## Validation Rules

After C++ changes:

- verify includes and module dependencies;
- build `SlayTheSpireDemoEditor` when the build environment is available and the user has authorized it;
- run the smallest focused Automation suite relevant to the changed contract;
- run the aggregate regression gate when required and available;
- report failures instead of masking them;
- require user-side compile/Automation if the current environment cannot run UE.

Never infer Blueprint/UMG or PIE correctness from C++ Automation. Never claim build, PIE, packaged-game, Shipping exclusion or regression success unless that exact validation was run.

Exact suite totals are historical evidence, not permanent acceptance constants. Do not arithmetically combine owner suites run with different configured prefixes.

## Trusted Automation and Build Evidence

```text
Phase 5         13/13 PASS
Phase 6A        23/23 PASS
Phase 6B        12/12 PASS
Phase 6C         5/5 PASS
Phase 6UI-A0    20/20 PASS
Phase 6UI-A1    11/11 PASS
Phase 6UI-A3     8/8 PASS

Historical combined owner run 92/92 PASS

Phase 6UI-A2A    8/8 PASS
Phase 6UI-A2B    8/8 PASS
Phase 6UI-A2C    8/8 PASS
Phase 6UI-A2D1   3/3 PASS
Phase 6UI-A2D2   4/4 PASS
Phase 6UI-A2D3   4/4 PASS
Phase 6UI-A2D4   6/6 PASS
Phase 6UI-A2D5 focused 6/6 PASS
Phase6R expanded aggregate 100/100 PASS
Shipping exclusion PASS
Phase 6UI-A2N R3 review 4/4 PASS
Phase 6UI-A2N R4 focused PASS
Phase 6UI-A2N R5 focused 4/4 PASS
Phase 6UI-A2N R6 focused 5/5 PASS
Phase 6UI-A2N R7 focused 5/5 PASS
Phase 6UI-A2N R8 focused 6/6 PASS
Phase 6UI-A2N R9 focused 5/5 PASS
Phase 6UI-A2N R10 focused 5/5 PASS
Phase 6UI-A2N R11 candidate: A2D5 6/6, Native 35/35, WBP 3/3 PASS
Phase 6UI-A2N R12 cutover: A2D5 6/6, Phase6R 100/100, WBP 3/3, Shipping and PIE PASS
```

## Current UI-A2E Goal-Run Evidence — 2026-08-31

### StatusChanged visible Blueprint/PIE acceptance

On `main@8af9487`, the saved `WBP_BattleHUD` at SHA-256 `574FF058...` was exercised
in a capturable floating PIE session through the real Gameplay status and EndTurn
paths. `Strength#1` committed `2` as creation and `3` as a same-identity update while
remaining a single visible widget. Enemy `Weak#3` committed `2`, reduced to `1` on
the first real EndTurn using the same widget, then reduced `1 -> 0` on the second
EndTurn and disappeared without reappearing after completion. No duplicate or
A→B→A flashback was observed, later Records completed, and the controller returned
to the ready/Idle state. StatusChanged creation, update/reduction, and removal are
therefore fully validated on that saved asset.

### Batch 2 visible Blueprint/PIE acceptance — 2026-08-31

On the saved `WBP_BattleHUD` at SHA-256 `7BF7488D...`, a real one-cost card changed
Energy from `5/5` to `4/5` exactly once, with no duplicate same-cost
`EnergyChanged`. A real EndTurn produced four ordered Hand discard records and five
ordered Draw operations. The real shuffle producer then committed
`MovedCardCount=5`, Draw `0 -> 5`, Discard `5 -> 0` exactly once, after which Draw
continued to a final Draw count of `4`. The queue emptied, the controller returned
to `ReadStateReady / State=2`, and input remained usable.

After the one Batch 2 architecture review, five P1 Blueprint wiring findings were
fixed and the HUD was recompiled/saved: Energy text format, Energy Cancel restore,
CardZone card/ToZone validation, two DeckShuffled count comparisons, and PlayArea
transient-reference cleanup. The corrected graph passed the same focused real PIE
regression.

The batch-level `SlayTheSpireDemo.Phase6UIA2C` Automation run completed 8 tests with
5 succeeded, 3 succeededWithWarnings, 0 failed, and 0 notRun. The warnings were the
expected rollback/fail-soft cases. This was not a final-head A2D5, Phase6R, or
Shipping-exclusion run.

### Batch 3 terminal Blueprint/PIE acceptance — 2026-08-31

On saved `WBP_BattleHUD` SHA-256 `24BA3F8B...`, real gameplay produced Victory
(enemy `29/100 -> 0/100`) and Defeat (player `2/80 -> 0/80`); the formal terminal
surface showed `胜利` and `战斗失败` only after preceding committed Records.

Two isolated real `UEDPIE` scenarios then used existing authoritative testing
producers through a temporary Editor-only harness. The EndTurn structural failure
produced seven ordered Records with exactly one final ResolutionFault, entered the
faulted State/Outcome, and showed the formal Overlay with `战斗结算异常`. A forced
presentation freeze failure instead published no ResolutionFault Envelope, left
Gameplay in PlayerTurn with `Outcome=None`, and kept the terminal Overlay collapsed.
The harness constructed no Record/Payload, was removed afterward, and the standard
Editor build succeeded with no C++ diff.

The one Batch 3 architecture review found no P0/P1/P2 issue. The one focused
`SlayTheSpireDemo.Phase6UIA2C` run completed 8 tests with 5 succeeded,
3 succeededWithWarnings, 0 failed, and 0 notRun. It is not final-head evidence.

### Batch 4 Cancel/Reconcile and full PIE acceptance — 2026-08-31

On saved `WBP_BattleHUD` SHA-256 `990125C9...`, Cancel now clears the active timer,
restores the type-specific historical ViewModel surface, and enters one single-
direction local cleanup tail. The tail clears card/status transient references,
Damage/Block target flags, active type, and active token, and never calls normal
completion Notify. One independent architecture review initially blocked four P1
wiring errors; all were corrected, recompiled/saved/reloaded, and the directed final
review passed with no remaining P0/P1.

Real PIE Scenario A used Strike (`Energy 5/5 -> 4/5`, Enemy `100/100 -> 94/100`),
Scenario B used Uppercut and two real EndTurn requests (Weak/Vulnerable `2 -> 1 -> 0`
with no duplicate), and Scenario C exercised the full discard/draw/shuffle EndTurn
macro before returning to PlayerTurn. The accepted Victory/Defeat and isolated
ResolutionFault/PresentationUnavailable runs supply Scenario D/E evidence.

A temporary Editor-only PIE Automation harness used the formal
`ViewModel->RequestEndTurn()` request, waited until the Controller owned a real active
token, then called public `WidgetInstance->SkipPresentation()`. It verified input
locked/Resolving before Skip, no waiting or backlog after reconcile, cleared Blueprint
transient/type/token fields, rejection of the stale token beyond the timer window,
a subsequent real request completing normally, and final Idle/input unlocked. It
constructed no Record/Payload, was deleted afterward, and the standard Editor build
succeeded with no Source diff. At this batch boundary the final-head gates had not
yet run; their later seal evidence follows.

### Final-head UI-A2E / UI-A2 seal gates — 2026-08-31

```text
Implementation commit 81cbfb6af09a52f96ececff597491c5bfcc3665f
WBP_BattleHUD SHA-256 990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

Phase6UIA2D5: exactly 6/6 successful, 0 failed, 0 notRun
Report: Saved/AutomationReports/FinalA2D5/index.json

Phase6R prefixes: 13+23+12+5+8+8+8+3+4+4+6+6 = 100/100 successful
0 failed, 0 notRun, all Editor exits 0
Reports: Saved/AutomationReports/FinalSeal_Phase5 through FinalSeal_Phase6UIA2D5

Clean-worktree Win64 Shipping build: exit 0
Forbidden SlayTheSpireDemoTests / Phase6ATest artifact hits: 0
Runtime UPhase6ATest hits: 0
```

An earlier nonexistent single prefix `SlayTheSpireDemo.Phase6R` matched zero tests
and is not aggregate evidence. The valid aggregate is the formal twelve-prefix
workflow above. Likewise, a first Shipping scan in the main workspace was rejected
because it saw pre-existing Editor-test artifacts; the valid Shipping gate used a
clean detached worktree at the same implementation commit, matching the workflow's
clean-checkout boundary. That worktree was removed after validation.

With the saved Blueprint/PIE evidence plus these final-head gates, UI-A2E and UI-A2
are **COMPLETE / VALIDATED / SEALED**.

### Phase 6UI-A2N R2 Native HUD shell — 2026-08-31

R2 implementation commit `d15287ec068f699390a4f64cfab824dcbe53980b`
adds only the Native HUD/Card/Status shells, their Designer-backed duplicate assets,
and the non-production `L_BattleTest_Native` map. Production remains on
`L_BattleTest` and `WBP_BattleHUD_C`.

```text
UE 5.8 project-file regeneration: PASS
SlayTheSpireDemoEditor Win64 Development build: PASS
Native HUD/Card/Status Blueprint compile + save: PASS

WBP_BattleHUD_Native: parent UBattleHUDWidget, 75 Designer Widgets,
  one empty EventGraph, 23 required bindings, 6 optional bindings
WBP_BattleCard_Native: parent UBattleCardWidget, 20 Designer Widgets,
  one empty EventGraph
WBP_BattleStatus_Native: parent UBattleStatusWidget, 4 Designer Widgets,
  one empty EventGraph

Native PIE map: /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
runtime WidgetClass: WBP_BattleHUD_Native_C
runtime WidgetInstance: WBP_BattleHUD_Native_C_0
ViewModel / PresentationController assembly: created through the existing Presenter
Native binding / ensure / Blueprint / UMG errors: 0

Focused SlayTheSpireDemo.Phase6UIA2A:
8 total, 3 succeeded, 5 succeededWithWarnings, 0 failed, 0 notRun
Report: Saved/AutomationReports/R2FocusedPhase6UIA2A/index.json
```

The independent R2 architecture review found no P0/P1 blocker. It recorded one
non-blocking migration residue: duplicated assets still contain unexecuted Legacy
member variables even though their business graphs are empty. Those variables must
be taken over or removed in the applicable later ownership phase; R2 does not expand
into the R4/R5/R9 behavior or the R14 cleanup boundary.

### Phase 6UI-A2N R3-A Static HUD and long-lived delegates — 2026-08-31

R3-A implementation commit is based on `e0ac820245e8ea93128507f058316e32c5aaf427`
and changes only `UBattleHUDWidget` static refresh and long-lived input ownership.
Production remains on `L_BattleTest` / `WBP_BattleHUD_C`; the Native checks use only
`L_BattleTest_Native` / `WBP_BattleHUD_Native_C`.

```text
SlayTheSpireDemoEditor Win64 Development build: PASS (Result: Succeeded)
CompileAllBlueprints: PASS (0 errors, 0 warnings, 0 failed blueprints)
Native PIE: PASS
  initial Player 80/80, Enemy 100/100, Energy 5/5
  TestAttack -> Enemy 94/100, Energy 4/5
  EndTurn -> real turn-ending commit, next ReadStateReady, Player 74/80, Energy 5/5
  Draw / Discard / Exhaust count surfaces remained ViewModel-consistent
  target handler -> frozen "Choose a legal target." feedback
  Confirm and Cancel handlers each invoked once on Native instance
  enemy inspection surfaced frozen name and cleared cleanly
NativeOnBattleHUDViewModelChanged: Native refresh only; no Legacy BP refresh
delegates: one NativeConstruct AddUniqueDynamic boundary, matching NativeDestruct removal
Legacy HUD/Card/Status hashes: unchanged from sealed baseline
```

Runtime log evidence is in `Saved/Logs/SlayTheSpireDemo.log`; local PIE captures are
under `Saved/Screenshots/WindowsEditor/`. R3-A deliberately did not rerun A2D5,
Phase6R or Shipping. R3-A is **COMPLETE / VALIDATED**; R4 remains NOT STARTED.

After the final PIE, the Editor returned to formal `L_BattleTest`; its Presenter and
the `BP_BattleHUDPresenter` default still resolve to `WBP_BattleHUD_C`. The three
Legacy WBP hashes remain the sealed R0 values.

### Phase 6UI-A2N R3-A review fixes — focused validation — 2026-08-31

The R3 review found two parity gaps: combatant inspect did not rebuild the optional
status tooltip from frozen `CombatantView.Statuses`, and Block `0` left the Designer
shield badge visible. The review-fix branch corrected only those Native static HUD
surfaces and added permanent Editor-only probes; it did not enter R4 Hand/Card,
R7 Damage playback, or R9 formal Status-row lifecycle.

The user rebuilt the saved review-fix branch on UE 5.8 and ran the focused prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge                 PASS
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip              PASS
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal                   PASS
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable    PASS

4/4 PASS
```

The focused evidence proves that zero/positive/zero Block toggles the complete
Designer badge, inspect receives the frozen Status DTO and clears cleanly, all four
Terminal outcomes render from ViewModel state, and PresentationUnavailable remains
input-locked and visually distinct from ResolutionFaulted. The Editor build for this
saved review-fix head also passed.
This closes the R3 review findings. It does not replace the earlier Native WBP/PIE
acceptance; together they keep R3-A **COMPLETE / VALIDATED**.

### Phase 6UI-A2N R4 Native Card / Hand — 2026-08-31

R4 moved only formal Hand/Card display and request ownership into the Native stack.
The user completed Editor Build, `WBP_BattleCard_Native` and `WBP_BattleHUD_Native`
compile, focused R4 Automation, and Native PIE interaction acceptance. Exact
RuntimeId selection, target/cancel behavior, accepted single submission, no duplicate
card callback, and the R3 zero-Block regression all passed. Production remained on
Legacy and committed Record playback remained immediate-fallback.

R4 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R4NativeCardHandValidation.md`.

### Phase 6UI-A2N R5 Native Playback Kernel — 2026-08-31

R5 added only the Native HUD local playback ownership kernel: exact active Token and
Record type, local finish timer, exact-token Finish/Cancel, failed-Begin rollback,
and destruction cleanup. It did not migrate any real Record visual or copy Controller
queue/reducer/WorkingSnapshot/generation/timeout authority into the HUD.

The first corrected-build cycle exposed and fixed one UE5.8 delegate-binding mismatch:
the finish timer now uses a weak lambda that captures the exact Token by value. The
user then completed the final R5 gates:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: PASS
SlayTheSpireDemo.Phase6UIA2N.R5: 4/4 PASS
L_BattleTest_Native minimal PIE smoke: PASS
```

The focused suite validates unsupported/failed Begin zero side effects, exact and
wrong-token Cancel, Cancel without normal completion Notify, duplicate/stale Finish,
old/new Token isolation, local timer ownership, and NativeDestruct cleanup. The PIE
smoke confirmed normal Native HUD/Hand startup, card-selection Cancel, EndTurn,
continued interaction, and no crash, permanent lock, duplicate Hand or blank HUD.

R5 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R5NativePlaybackKernelValidation.md`.

### Phase 6UI-A2N R6 Energy / Block / Shuffle — 2026-08-31

R6 migrated only `EnergyChanged`, `BlockChanged` and `DeckShuffled` into the Native
HUD. The handlers consume frozen Record payload plus the required frozen historical
Before state, reuse the R5 exact-token kernel, render frozen After on Begin/Finish,
and restore frozen Before on exact Cancel without normal completion Notify.

Validation evidence:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: NOT REQUIRED
  (no runtime reflected binding/API contract changed)
SlayTheSpireDemo.Phase6UIA2N.R6: 5/5 PASS
L_BattleTest_Native minimal R6 PIE: PASS
```

Focused coverage includes Player/Enemy Block and zero-badge behavior, Energy and
Shuffle Before/After/Cancel, invalid payload/target/token zero-side-effect Begin,
stale/duplicate Finish, wrong/exact Cancel, exact completion ownership cleanup and
NativeDestruct cleanup. The manual PIE confirmed Energy, Block and real Shuffle final
values with no visible flashback, duplicate display, permanent Input Lock or abnormal
HUD state.

R6 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R6NativeEnergyBlockShuffleValidation.md`.

### Phase 6UI-A2N R7 Native Damage — 2026-08-31

R7 migrated only the committed `Damage` Record into the Native HUD. The handler
resolves the exact frozen `TargetPresentationId`, validates historical HP/Block Before
against the frozen ViewModel, consumes the Record's `IncomingDamage`, `HPBefore /
HPAfter` and `BlockBefore / BlockAfter` directly, and never derives committed HP or
Block outcomes from IncomingDamage.

Validation evidence:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: NOT REQUIRED
  (no runtime reflected binding/API contract changed)
SlayTheSpireDemo.Phase6UIA2N.R7: 5/5 PASS
L_BattleTest_Native minimal R7 Damage PIE: PASS
```

Focused coverage includes Player and Enemy targets, ordinary Damage, full Block
absorption with unchanged HP, lethal overkill, exact Cancel historical restore,
wrong-token Cancel, stale/duplicate Finish, next-Record isolation, invalid target /
payload / Before / Token zero-side-effect Begin, and NativeDestruct cleanup. The
manual PIE confirmed one correctly targeted Damage number/feedback, correct final
HP/Block, no duplicate/flashback, and no permanent Input Lock.

R7 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R7NativeDamageValidation.md`.

### Phase 6UI-A2N R8 Native Card Lifecycle — 2026-09-01

R8 migrated the committed `CardPlayed` and `CardZoneChanged` facts together while
keeping them independent exact-token playback units. All card visuals consume only
the frozen card snapshot, Record indices/counts and frozen ViewModel Before state.
Presentation cards are noninteractive, `HitTestInvisible`, and never become formal
Gameplay-playable Hand cards.

The initial implementation correctly serialized one-card-at-a-time DrawPile-to-Hand
presentation, but the first manual pass found that the non-Draw paths still used
immediate visibility changes. The correction added the required Slay-the-Spire-like
movement/retirement cues:

```text
Hand -> PlayArea:                  move / scale / fade into centered PlayArea
Hand -> DiscardPile:               move / fade toward DiscardPile
PlayArea -> DiscardPile:           move / fade toward DiscardPile
PlayArea -> Exhaust/RemovedPile:   scale / fade out at PlayArea
DrawPile -> Hand:                  exactly one Record/card moves to exact ToIndex
```

A later narrow review found one P1 cleanup gap: after exact `CardPlayed` Finish, the
cross-Record `NativePlayedCardWidget` intentionally survives for a later PlayArea
destination. If a subsequent Record was abandoned through `SkipPresentation` /
fail-safe exact Cancel, that retained PlayedCard could survive Controller collapse.
The exact native Cancel boundary now retires any retained PlayedCard after the
current Record type-specific Cancel and before local ownership is cleared. Wrong or
stale Token cancellation still returns before this cleanup.

Validation evidence after the P1 fix:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native / WBP_BattleCard_Native targeted compile: NOT REQUIRED
  (no production reflected binding/API contract changed)
SlayTheSpireDemo.Phase6UIA2N.R8: 6/6 PASS
L_BattleTest_Native corrected minimal R8 Card lifecycle PIE: PASS / sticky
```

Focused coverage includes exact RuntimeId/CardId/HandIndex identity, duplicate
RuntimeId/wrong CardId rejection, no duplicate Energy visual, supported/unsupported
zone pairs, exact/stale Token behavior, Finish/Cancel historical cleanup,
noninteractive presentation cards, strict per-Record consecutive draws,
transform/opacity progress for every migrated lifecycle path, NativeDestruct cleanup,
and the new `Zone.SkipClearsRetainedPlayedCard` regression proving Skip clears both
the active Draw transient and the previously retained PlayedCard.

The corrected manual PIE confirmed Hand-to-PlayArea-to-Discard movement, Exhaust
disappearance at PlayArea, end-turn/manual discard movement, strictly serial draws,
correct final Hand/HUD state, and no flashback, duplicate card, transient leak,
abnormal HUD, or permanent Input Lock. It remained valid after the P1 fix because
normal visual paths did not change.

R8 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R8NativeCardLifecycleValidation.md`.

### Phase 6UI-A2N R9 Native Status Lifecycle — 2026-09-01

R9 migrated formal Native Status-row ownership and committed `StatusChanged`
presentation only. `UBattleStatusWidget` owns a frozen `FBattleHUDStatusView` and
Designer-backed amount/icon rendering. HUD lookup uses the sealed identity:

```text
TargetPresentationId + StatusId + RuntimeSequence
```

Create requires the exact identity to be absent. Increase/reduction/removal require
one exact historical ViewModel status and one exact formal Widget; update/reduction
reuse the same Widget and removal collapses only that exact identity. Exact Cancel
rebuilds both Player and Enemy formal Status rows from the historical ViewModel and
never reverse-computes `B -> A`.

Validation evidence:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleStatus_Native targeted compile: PASS
SlayTheSpireDemo.Phase6UIA2N.R9: 5/5 PASS
L_BattleTest_Native minimal R9 Status lifecycle PIE: PASS
```

Focused coverage includes frozen DTO/identity, create, increase, exact Widget reuse,
`2 -> 1` reduction, `1 -> 0` removal, same `StatusId` with new `RuntimeSequence`,
invalid target/identity/flags/reason zero-side-effect fallback, Player+Enemy historical
Cancel rebuild, wrong-token Cancel, stale/duplicate Finish, next-Record isolation and
NativeDestruct cleanup. The manual PIE accepted the real Status lifecycle with one
correct row/icon/amount, exact-identity reuse, disappearance at zero, coherent
row/icon/tooltip presentation, no `A -> B -> A` flashback, no duplicate Status, no
abnormal HUD and no permanent Input Lock.

R9 is **COMPLETE / VALIDATED**. Detailed evidence is in
`docs/R9NativeStatusLifecycleValidation.md`.

### Phase 6UI-A2N R11 temporal protocol parity — 2026-09-01

The temporary Editor-only R11 PIE harness executed the two formal temporal paths on
both production Legacy `L_BattleTest / WBP_BattleHUD_C` and isolated Native
`L_BattleTest_Native / WBP_BattleHUD_Native_C`:

```text
Legacy active Skip + catch-up + Input Unlock: PASS
Legacy active Cancel + stale callback + Input Unlock: PASS
Native active Skip + catch-up + Input Unlock: PASS
Native active Cancel + stale callback + Input Unlock: PASS
```

Every run began with a real `Widget->EndTurn()` request and observed a real active
Controller Token. Skip used `Widget->SkipPresentation()`. Cancel used the formal
`ExpireActivePlaybackForTesting() -> HandleActiveTimeout()` path; after distinct
Token B became active, the harness deliberately delivered Token A through
`Widget->NotifyPresentationFinished(A)` and proved that B remained active.

Final assertions proved zero Controller backlog/waiting, completion-watermark
catch-up, exact ViewModel equality with the latest frozen FinalSnapshot, Idle and
unlocked input, and acceptance of a second real Widget EndTurn request. The Editor
build for the test-only harness passed.

The unattended no-rendering runs own the deterministic state, ownership, stale-token
and input contracts. The user then ran both commands on Legacy and Native PIE and
confirmed the remaining visual parity on 2026-09-01: active Skip produced no
`A -> B -> A` flashback or duplicate Hand/Status; active Cancel/stale produced no
abandoned-visual return, duplicate visual or disturbance of later playback.

Therefore the complete R11 active Skip, active Cancel, stale callback, catch-up and
Input Unlock parity scope is PASS on both stacks. The user also confirmed the full
R11 candidate gates on 2026-09-01:

```text
SlayTheSpireDemoEditor Win64 Development: PASS
SlayTheSpireDemo.Phase6UIA2D5: exactly 6/6 PASS
SlayTheSpireDemo.Phase6UIA2N: exactly 35/35 PASS
WBP_BattleHUD_Native compile/save: PASS
WBP_BattleCard_Native compile/save: PASS
WBP_BattleStatus_Native compile/save: PASS
```

After closure, the temporary R11 PIE command source and its sole Editor-test
`UnrealEd` dependency were removed. Codex reran the Editor build after that cleanup;
it passed. No Runtime source, existing Automation fixture, Blueprint asset, map or
production configuration changed, so the other passing Gates remain sticky.

R11 is **COMPLETE / VALIDATED**.

### Phase 6UI-A2N R12 production cutover — 2026-09-01

R12-A changed only the unique production-map Presenter `WidgetClass` from
`WBP_BattleHUD_C` to `WBP_BattleHUD_Native_C`. The isolated cutover commit is
`de788c5b68e06827f8fdba3b83858f86a385bdeb`; no Runtime/C++, Native implementation,
Legacy asset or unrelated production asset was included.

AUTOMATED GATES:

```text
SlayTheSpireDemoEditor Win64 Development: PASS
Native WBP compile/save/reopen: 3/3 PASS, BS_UP_TO_DATE, 0 errors
SlayTheSpireDemo.Phase6UIA2D5: exactly 6/6 PASS, 0 failed, 0 notRun
Formal current-head Phase6R workflow: exactly 100/100 PASS, 0 failed, 0 notRun
Clean-worktree Win64 Shipping: PASS
Forbidden Shipping test artifacts: 0
Runtime testing/harness hits: 0
```

MANUAL PIE GATES:

The user completed the formal production-map `L_BattleTest` acceptance on
2026-09-01 and confirmed Scenario A-E, Victory, Defeat, active Skip, active timeout
Cancel, stale callback rejection and Input Unlock all PASS. The temporal checks
used real Widget EndTurn requests, real Controller waiting Tokens, the formal
timeout path and a deliberately late Token A callback; both commands reported PASS
and the user confirmed no flashback, duplicate, transient return, terminal rollback,
premature unlock or permanent Input Lock.

The temporary Editor-test-only R12 PIE command source was never committed and was
deleted after manual acceptance. The affected Editor build and clean-worktree
Shipping exclusion Gate were rerun on the no-harness tree and passed. Complete R12
evidence is recorded in `docs/R12NativeProductionCutoverValidation.md`.

R12-A is **COMPLETE** and R12-B is **COMPLETE / VALIDATED**. Native HUD is the
production default and Legacy assets remain retained.

### Phase 6UI-A2N R13 objective stabilization — 2026-09-01

R13-M1 — Native Production Stabilization began at
`76d411a21c042a86d1e7a4c608a67ae10c724ea2`. Commit `fe7fe4e` supplied the required
real post-cutover Native-only UI change: the Presenter default now selects
`WBP_BattleHUD_Native_C`, and Native HUD transient Card/Status ownership no longer
uses Legacy concrete widget types. Legacy assets were not modified.

AUTOMATED GATES:

```text
SlayTheSpireDemoEditor Win64 Development: PASS
SlayTheSpireDemo.Phase6UIA2N.R13: exactly 1/1 PASS, 0 failed, 0 notRun
Formal current-head Phase6R: exactly 100/100 Success, 0 failed, 0 notRun
Final post-harness-cleanup SlayTheSpireDemo Win64 Shipping: PASS
Shipping forbidden artifact hits: 0
Runtime temporary harness hits: 0
Editor-test temporary harness hits: 0
Production runtime Legacy HUD/Card/Status dependency: 0 — PASS
```

MANUAL PIE GATES:

The user completed the final production-map `L_BattleTest` pass on 2026-09-01 and
confirmed Scenario A-E PASS. The active Skip and active timeout Cancel commands used
real Widget EndTurn requests and real active Tokens; logs confirmed Skip catch-up,
exact Cancel, stale Token A rejection, queue catch-up and a successful real EndTurn
after input unlock. The apparent second HP loss in the Skip log came from this
deliberate second EndTurn input probe, not duplicate playback of one Damage Record.

The temporary Editor-only R13 PIE source and temporary test-module dependency were
removed and never committed. The final no-harness Editor and Shipping builds passed.
Native remained the production default throughout R13-M1 and Legacy runtime fallback
was `NO`.

R13 is **COMPLETE / VALIDATED**. R14-A is **COMPLETE / VALIDATED**; R14-A1 and R14-A2 are
**COMPLETE / VALIDATED**, R14-B is
**NOT REQUIRED / NOT AUTHORIZED**, Legacy assets remain retained, and UI-A3 is **NOT STARTED**.

### Phase 6UI-A2N R14-A safe cleanup — 2026-09-01

R14-A1 removed the confirmed-unreferenced Native Card/HUD binding helper accessors;
its Editor Build, focused R3/R4 Automation, and production-map PIE smoke passed.

R14-A2 removed thirteen zero-reference migration-only member variables from the
three Native Blueprint duplicates. UE asset-level inspection found empty business
graphs, zero serialized property bindings/animations, and no Designer dependency for
the candidates. Each Native asset passed compile/save/fresh-reopen compile with
`BS_UP_TO_DATE` and zero errors. Legacy assets were not modified.

AUTOMATED GATES:

```text
SlayTheSpireDemoEditor Win64 Development: PASS
SlayTheSpireDemo.Phase6UIA2N.R4: exactly 1/1 PASS, 0 failed, 0 notRun
SlayTheSpireDemo.Phase6UIA2N.R9: exactly 5/5 PASS, 0 failed, 0 notRun
R13.AssetReferences.NativeProductionClosure: exactly 1/1 PASS, 0 failed, 0 notRun
Production runtime Legacy HUD/Card/Status dependency count: 0
```

MANUAL PIE GATE:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest production smoke: PASS
```

The user confirmed the production smoke on **2026-09-01**: Native HUD/Hand/Energy/
HP/pile surfaces, one ordinary attack Card/Damage presentation, final Card zone, and
post-catch-up input recovery were correct, with no duplicate, flashback, Native
binding error, or Blueprint runtime error.

Detailed evidence: `docs/R14ASafeCleanupValidation.md`.

### Deprecated Legacy battle-UI asset relocation — 2026-09-01

The retained deprecated Legacy assets were moved through Unreal AssetTools, without
deletion or runtime reactivation, from `/Game/SlayTheSpireDemo/UI/Widgets/` to:

```text
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleHUD
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleCard
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleStatus
```

The old package paths are absent, the new packages load, scoped Redirector count is
zero, and all three relocated Legacy WBP assets passed compile/save/fresh-reopen
compile as `BS_UP_TO_DATE`. The relocated Legacy HUD's Card/Status references point
to the relocated packages.

AUTOMATED GATES:

```text
SlayTheSpireDemoEditor Win64 Development: PASS
SlayTheSpireDemo.Phase6UIA2N.R13.AssetReferences.NativeProductionClosure:
  exactly 1/1 Success, 0 failed, 0 notRun
Production runtime relocated Legacy HUD/Card/Status dependency count: 0
```

The permanent test uses the new `/UI/Out/Legacy/` package constants and asserts all
three relocated packages exist.

MANUAL PIE GATE:

The user confirmed the production `/Game/SlayTheSpireDemo/Maps/L_BattleTest` smoke
on **2026-09-02**. Native HUD and Hand creation, one ordinary Card presentation, and
post-catch-up input recovery passed. No Blueprint runtime error or missing
asset/package warning was observed.

On local branch `codex/A2E-continue`, with the saved StatusChanged update/reduction and Cancel-restoration HUD asset plus uncommitted documentation changes, the focused `SlayTheSpireDemo.Phase6UIA2D5` suite was rediscovered as exactly six tests and actually run:

```text
Terminal.Defeat             PASS
Terminal.ResolutionFault   PASS
Terminal.Victory           PASS
CardStatusIntegration      PASS
StatusLifecycle            PASS
TurnCycleOrdering          PASS

6 passed / 0 failed / 0 skipped
total duration 0.108884 s
```

This historical run was current-working-tree focused regression evidence only. It is
not `final-head`; the current Status and Scenario A-E Blueprint/PIE evidence is
recorded above, and the valid final-head A2D5 run is recorded in the seal section.

## Trusted Manual Evidence

- Normal UI player → enemy → player turn loop passed in PIE.
- Self-target Defend → highlighted Player selection passed in PIE.
- Packaged Defend dynamic `{Block}` description passed.

These manual results predate unified UI-A2 committed-record playback and therefore do **not** close UI-A2E.

## Current Acceptance Boundary

UI-A2A/A2B/A2C/A2D C++ committed-presentation contracts are sealed by focused and
aggregate Automation evidence. UI-A2E unified Blueprint/UMG routing and actual PIE
Scenario A-E/Cancel acceptance are now validated on HUD hash `990125C9...`.
UI-A2E and UI-A2 are **COMPLETE / VALIDATED / SEALED** on implementation commit
`81cbfb6` after A2D5 exactly 6, Phase6R 100/100, and clean-worktree Shipping
exclusion all passed.

A2N migration status is now:

```text
R0-R13 COMPLETE / VALIDATED
Native HUD = production default
Legacy assets retained
R14-A COMPLETE / VALIDATED
R14-A1 COMPLETE / VALIDATED
R14-A2 COMPLETE / VALIDATED
R14-B NOT REQUIRED / NOT AUTHORIZED
UI-A3 NOT STARTED
```

Validated A2E scenarios include:

- ordinary card Damage;
- card plus Status creation/update/reduction/removal;
- complete EndTurn macro Envelope, including Block/Energy/zone/shuffle behavior as applicable;
- Victory and Defeat;
- genuine ResolutionFault distinct from PresentationUnavailable;
- input remains locked during playback and unlocks only after catch-up to the newest matching revision.

Use `docs/UIA2ERemainingSteps.zh-CN.md` for historical UI-A2E execution evidence and
`docs/ValidationExecutionPolicy.md` for current A2N validation budgeting and manual/
automated Gate ownership.

## User-Action Boundary

When UE Editor work is required but unavailable to the agent, label it `USER ACTION REQUIRED` and provide exact asset/menu paths, graph/function names, nodes, pins, property values, compile/save order, expected results and requested logs/screenshots.
