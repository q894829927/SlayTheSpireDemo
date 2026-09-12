# Codex Goal Checkpoint — Interior Portals (interrupted)

## Current resumable task — Interior portals, 2026-09-12

HEAD: `18d4686d9113aacd9fb655426e3b7a7bfcfcd830`.

User requests a Portal-like mechanism in `/Game/House/L_Interior_LivingKitchen` and prefers MCP over desktop control. Desktop Computer Use was stopped by the user with Escape; no further desktop actions were issued. Use MCP for subsequent editor work.

Implemented but **not compiled or accepted**: `InteriorPortalMath`, `InteriorPortal`, `InteriorPortalSystem`, controller/HUD/input integration and three focused Automation tests. Design and gates: `docs/InteriorPortals.md`. No portal material assets or endpoint/system map instances have been created yet. The code still needs build diagnostics and review, especially recursive render correctness, near-plane crossing, upright capsule transitions, scoped collision restoration, physics exit obstruction and partial-body rendering. Do not claim a finished or visually identical portal mechanism.

Existing user modifications at entry: map, `InteriorChildCharacter.cpp/.h`, untracked `Content/House/BP_CorridorSegment.uasset`. Preserve these. MCP inspection found PlayerStart `(1300,300,100)`, yaw 90, in the corridor extension. Author initial portals near there, not in the old living-room spawn. Detailed transforms/bounds: `Saved/PortalSceneInspection.json`.

Validation: bundled UE 5.8 project-file generation succeeded. Editor build blocked before compilation by active Live Coding, evidence `Saved/Logs/InteriorPortalsBuild.log`; no Automation or portal PIE tests executed. Map and corridor Blueprint were confirmed saved via MCP. MCP Slate Click close and Alt+F4 returned true but editor process 63544 remained running; do not assume closure.

Next: close editor normally using MCP if possible (or ask user to close it), execute prescribed Development Editor build, fix compilation failures, create and save procedural portal material + pair/system + surface references using UE-supported tooling, run focused tests and actual-map visual/runtime checks. MCP endpoint `http://127.0.0.1:8000/mcp`; helper `Saved/PortalMcp.ps1` handles JSON/SSE, session ID in `Saved/PortalMcpSession.txt` (reinitialize after restart). Discovered schemas are `Saved/Portal*Schema.json`. `ProgrammaticToolset.get_execution_environment` was read: scripts may only orchestrate registered tools with allowed standard modules. Do not bypass its sandbox. Slate observers: `observer_1`, `observer_2`; window `w1`, may become stale after restart.

`Saved/PortalEditorBridge.py` and `Saved/PortalEditorCommand.py` were written locally but **never executed or activated**. They are not evidence of editor edits. No plugin/config/build-setting changes, commits or generated-file staging were performed.

## Prior checkpoint retained below

Last updated: **2026-09-11**

Use `main` as the working branch unless a later explicit branch decision supersedes it.

## Current resumable task — Awakened One Monster Animation

The Awakened One enemy profile is implemented in the Native combatant Presentation widget and
is verified in `/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel`. The active profile uses
48 `Idle_2` frames, 8 `Hit` frames and 24 `Attack_1` frames imported under
`Content/SlayTheSpireDemo/UI/Textures/AwakenedOne`. `Damage` aimed at the enemy requests Hit;
committed non-player Attack damage aimed at the player requests the enemy Attack sequence.
The profile is Presentation-only and its durations/arrays are Blueprint-editable.

Completed: imported and saved all runtime textures; switched from the near-static `Idle_1` source
to visible `Idle_2`; set `bAnimateEnemyCharacter` on the combatant Blueprint; added enemy Attack
record mapping; and confirmed visible tail/eye changes in focused floating PIE on
`L_Battle_RuinedCitadel`. Bundled project-file generation and the Development Editor build passed
(`Saved/Logs/AwakenedOneMonsterAnimationFinalProjectFiles.log`,
`Saved/Logs/AwakenedOneMonsterAnimationFinalBuild.log`). Full scope and acceptance are recorded in
[AwakenedOneCharacterAnimation.md](AwakenedOneCharacterAnimation.md).
The combatant Widget Blueprint compiled and saved successfully through Unreal MCP.

Layout amendment completed through Unreal MCP: `WBP_BattleHUD_Native` now owns an
`EnemyCombatantCluster` Overlay containing both `Combatant_EnemyPresentation` and
`EnemyIntentPanel`. The intent uses centered top alignment with `55` padding, and only the enemy
instance uses a `1.18` bottom-center scale so the player remains unchanged. Both affected WBP
assets compiled/saved and focused `L_Battle_RuinedCitadel` PIE confirmed the larger enemy with
the intent directly above it. The unused `Img_PlayerCharacter` and `Img_EnemyCharacter` brushes
were removed from the Native Designer tree; the duplicate-image preview issue is resolved.

Next / USER ACTION REQUIRED: in the same level, end the player turn and observe the enemy
`Attack_1` pose during player damage, then play an Attack card to observe the enemy `Hit` pose.
Do not claim the final attack/hit visual gate until those interactions are observed.

## Previous resumable task — Ironclad Character Animation

The Ironclad character profile is implemented in the Native combatant Presentation widget.
The imported source is baked into 120 Idle frames, 8 Hit frames and one corpse texture under
`Content/SlayTheSpireDemo/UI/Textures/Ironclad`. `CardPlayed`, `Damage`, `Victory` and `Defeat`
records now request Attack, Hit, Victory and Defeat visuals after their validated playback start.
Attack is an Idle-frame lunge because the supplied Spine data has no authored attack animation;
Defeat uses the supplied corpse image. The frame durations, lunge, pulse, opacity, optional frame
arrays and enemy opt-in are Blueprint-editable on `WBP_CombatantPresentation`'s native parent.

Completed: native frame playback and deterministic elapsed-time selection; fallback restoration
to the authored `Img_Character` brush/transform; no Spine runtime dependency; docs in
[IroncladCharacterAnimation.md](IroncladCharacterAnimation.md). Bundled project-file generation
and Development Editor build passed (`Saved/Logs/IroncladCharacterAnimationProjectFiles.log`,
`Saved/Logs/IroncladCharacterAnimationBuild.log`).
`CompileAllBlueprints` completed with 0 errors and `WBP_CombatantPresentation` successful
(`Saved/Logs/IroncladCharacterAnimationBlueprints.log`; only existing unrelated warnings remain).

Next / USER ACTION REQUIRED: reopen Native `L_BattleTest_Native`, verify Idle loop, card-play lunge,
damage Hit, terminal Victory/Defeat, image alignment and the Blueprint disable fallback. No
manual PIE visual pass is claimed yet.

## Previous resumable task — Fan Hand / Attack Targeting

HEAD `268e3006b8f04e924af6d09a020c45460b35c103`; local uncommitted changes.
Scope: user-requested fan Hand, enlarged hover cards, gray/red single-enemy Attack
arrow, viewport-wide mouse-right cancellation for ordinary selection, Blueprint-tunable
Hand layout and predicted Draw→Hand fan targets; [execution/evidence](HandFanTargetingInteraction.md).
G8 stays deferred.

Completed: UBattleHandFanPanel with ordered Canvas slots, stable hover regions,
raised-card painting and precomputed incoming-card fan targets; UBattleTargetingArrowWidget with private imported textures,
frozen-view type gate and legal-enemy tint; Native HUD initialization/tick/input
integration, including Presenter-level viewport-wide right-click cancellation.
Formal Hand remains the sole current Hand visual, preserving G5/G6
Hidden slots. CardPlayed uses fan source scale for its visual start. New assets:
UI/Textures/Targeting/T_reticleArrow and T_reticleBlock. No Gameplay/plugin/Legacy
change; the Native module privately links SlateCore only for the FReply input boundary.
New tests cover Native asset installation and policy;
G5/Input fixtures use the actual fan host; stale R8 unsupported-pair test corrected.

Final generation/build PASS (`HandInteractionVerifiedProjectFiles.log`,
`HandInteractionVerifiedBuild.log` under Saved/Logs). The viewport-wide right-click
follow-up generation/build also passed (`RightClickGlobalFinalProjectFiles.log`,
`RightClickGlobalFinalBuild.log`), and its focused Automation passed 1/1:
`Saved/AutomationReports/RightMouseButtonCancel/index.json`; Presenter input-stack
installation passed in `Saved/AutomationReports/RightClickGlobal/index.json`. Final affected Automation
11/11 PASS, no warnings/failures/notRun, exit 0:
`Saved/AutomationReports/HandInteractionVerified/index.json`. Additional retained
evidence and intermediate failures are in the dedicated document; do not sum
overlapping counts. `git diff --check` PASS. No manual visual PASS.

The incoming-card prediction and Blueprint layout follow-up also passed standard
generation/build (`HandFanPredictionProjectFiles.log`, `HandFanPredictionBuild.log`) and
focused Automation **4/4** in `Saved/AutomationReports/HandFanPrediction/index.json`.

Next / USER ACTION REQUIRED: reopen Native L_BattleTest; inspect fan/hover at both
ends and overlaps, attack arrow gray over empty space/red over legal enemy, no
arrow for self/untargeted/non-Attack cards, left/right-click cancel/submit cleanup, multi-Exhaust and
Warcry continuity. Mark visual acceptance only on actual user evidence. Changes
are not committed; no further phase is automatically authorized.

## Earlier resumable state — Selection Presentation G0-G7 sealed

Selection Presentation has no active implementation stage after G7.

```text
G0 — COMPLETE / VALIDATED / SEALED
G1 — COMPLETE / VALIDATED / SEALED
G2 — COMPLETE / VALIDATED / SEALED
G3 — COMPLETE / VALIDATED / SEALED
G4 — generic SingleRecord transition engine retained as foundation;
     isolated pre-G5 compatibility-position PIE remains HISTORICAL FAIL
G5 — COMPLETE / VALIDATED / SEALED
G6 — COMPLETE / VALIDATED / SEALED
G7 — COMPLETE / VALIDATED / SEALED
G8 — SEPARATE DEFERRED INITIATIVE / NOT IMPLEMENTED / NOT AUTHORIZED
```

Do **not** resume from G1-G7. Do not automatically begin G8.

Current-status amendment for older design/summary banners:

```text
docs/SelectionPresentationG7SealAmendment.md
```

That amendment supersedes only stale stage/status/resume wording such as `G6 NEXT ACTIVE`; it does not replace the durable architecture contracts in the original constraints/design/implementation documents.

## G6 final authority

Dedicated record:

```text
docs/SelectionPresentationG6Execution.md
```

Final G6 evidence:

```text
[x] UE 5.8 Development Editor build PASS
[x] SlayTheSpireDemo.SelectionPresentation.G6 focused Automation 4/4 PASS
[x] transactional N-child visual preflight
[x] atomic SelectionArea -> Transition cohort ownership
[x] Transition -> ConsumedPendingReducer lifecycle
[x] visual co-presentation with chronological reducer order preserved
[x] exact G5 sequential fallback
[x] Native L_BattleTest Player multi-select N>1 PIE PASS
[x] simultaneous start from exact SelectionArea positions
[x] no flashback / duplicate / ghost / clipping
[x] correct final destination state
[x] input and later selection remain usable
[x] ordinary single-selection / Warcry path remains correct
```

## G7 final authority

Execution/seal record and final validation record:

```text
docs/SelectionPresentationG7Execution.md
docs/SelectionPresentationG7Validation.md
```

The G7 audit proved that the high-risk pre-G5 compatibility targets had already been removed from production Source during the coherent G4+G5 migration:

```text
ConfirmedCardCenters / confirmed-center runtime map
confirmed-position restoration as Selection continuity truth
Selection-subclass destination-specific animation ownership
old Selection-specific Hand -> DrawPile presentation shell
```

The current per-child transition engine, SelectionArea exact-object source resolver, ownership lifecycle and recovery paths are sealed production mechanisms and were retained.

G7 cleanup changed only:

```text
114f6e60a375d7c82548541f3088e66dad5d7296
cleanup(g7): remove obsolete selection test alias

4a3cff000b7e845aa4b5709222b9aebfebccbe86
cleanup(g7): normalize sealed transition comments
```

Final G7 Source diff:

```text
Source/SlayTheSpireDemo/UI/BattleHUDSelectionWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDCardTransitionWidget.h
```

No `.cpp` runtime branch, animation parameter, ownership mutation, Controller/reducer behavior, Gameplay rule, asset or map changed.

Final user-confirmed G7 validation on 2026-09-10:

```text
[x] UE 5.8 Development Editor build PASS as part of the prescribed local validation cycle
[x] SlayTheSpireDemo.SelectionPresentation.G6 focused regression PASS
[x] SlayTheSpireDemo.CardSelection.Presentation focused regression PASS
[x] Native L_BattleTest PIE validation completed
```

Therefore:

```text
G7 — COMPLETE / VALIDATED / SEALED
G8 — remains DEFERRED / NOT IMPLEMENTED
```

## Durable Selection Presentation invariants after G7

Preserve all of the following unless a separately authorized design explicitly changes them:

```text
Gameplay owns authoritative card zones and battle mutation
PresentationSequence/reducer chronology remains serial and authoritative
SelectionArea is Presentation ownership, not a Gameplay zone
one visible Presentation owner per RuntimeId
Hand -> SelectionArea -> Transition -> ConsumedPendingReducer -> Done
exact BattleId / SelectionGeneration / boundary scoping
exact completion watermark recovery
G5 SingleRecord fallback remains first-class correctness behavior
G6 Group preflight is transactional and all-or-nothing
future co-presented members do not reduce early
interleaved unrelated records retain chronological playback/reduction
Group timeout/cancel/replacement recovery remains exact-token scoped
no Effect/CardId-specific Selection destination animation branches
```

Do not restore `ConfirmedCardCenters` or confirmed-position reconstruction as a second continuity contract.

## G8 boundary

G8 early input / Presentation pipelining remains a **separate deferred initiative**. It is not the automatic continuation of G7.

Do not implement or silently enable:

```text
cross-Resolution early input
new overlapping interactive requests while older visual jobs are active
detached visual-job lifetime
cosmetic NonBlocking semantics
current formal HUD mutation by an older visual job
```

Starting G8 requires explicit authorization and a dedicated design/acceptance scope.

## Current Card Expansion status

Card Expansion remains separate from the completed Selection Presentation G0-G7 program.

```text
Wave 1A — Exhaust Fact Surface
→ COMPLETE / VALIDATED / SEALED

Wave 1B — Targeted Exhaust Primitive
→ COMPLETE / VALIDATED / SEALED

Wave 1C-A — Selection Primitive
→ COMPLETE / VALIDATED / SEALED

Wave 1C-B — Burning Pact first-consumer closure
→ COMPLETE / VALIDATED / SEALED
→ merged to main by PR #16

Wave 1C-C0 — Select-Exhaust Generalization
→ COMPLETE / VALIDATED / SEALED
→ final authority: docs/CardExpansionWave1CC0Execution.md

Wave 1C-C1 — Hand -> DrawPileTop / Warcry capability
→ IMPLEMENTED
→ MERGED TO main by PR #18
→ merge commit ffbc164905a875bea5c9ab3dfe0a07df5068b8cc
→ Development Editor build PASS recorded
→ Wave1CC1.DrawPileTop focused Automation 7/7 PASS recorded
→ shared later Selection Presentation visual path user-accepted
→ FINAL STANDALONE C1 USER SEAL RECORD NOT PRESENT

Former C1 True Grit plan
→ SUPERSEDED / NOT ACTIVE
```

Do not infer a standalone C1 `COMPLETE / VALIDATED / SEALED` state from PR #18 or shared Selection Presentation evidence.

This checkpoint does **not** authorize a new Card Expansion slice. Wave 1D Reactive Exhaust Powers, production True Grit content, Card Trigger Source Expansion, multi-enemy work, Exhume and other capability waves require their own explicit scope/authority.

## Current authority chain

Selection Presentation:

```text
docs/CardSelectionPresentationConstraints.md
docs/SelectionPresentationGroupDesign.md
docs/SelectionPresentationGroupImplementationPlan.md
docs/SelectionPresentationG7SealAmendment.md
docs/SelectionPresentationG0Execution.md
docs/SelectionPresentationG1Execution.md
docs/SelectionPresentationG2Execution.md
docs/SelectionPresentationG3Execution.md
docs/SelectionPresentationG4Execution.md
docs/SelectionPresentationG5Execution.md
docs/SelectionPresentationG6Execution.md
docs/SelectionPresentationG7Execution.md
docs/SelectionPresentationG7Validation.md
docs/ValidationExecutionPolicy.md
docs/Validation.md
```

Card Expansion:

```text
docs/IroncladCardArchitecturePlan.md
docs/IroncladCardArchitecturePlanWave1Amendment.md
docs/CardExpansionWave1AExhaustFactSurface.md
docs/CardExpansionWave1BTargetedExhaustPrimitive.md
docs/CardExpansionWave1CSelectionPrimitive.md
docs/CardExpansionWave1CC0Execution.md
docs/CardExpansionWave1CC1WarcryHandToDrawPileTopPlan.md
docs/CardExpansionWave1CC1ConfigurableDrawPileTopAmendment.md
```

## Resume instruction

When work resumes, first identify the newly authorized goal instead of continuing Selection Presentation automatically.

For any Selection Presentation regression, read the G6/G7 execution records plus the durable constraints/design documents and the G7 seal amendment, and preserve the sealed G0-G7 invariants. For new G8 work, require explicit authorization before implementation.

Historical execution/design files may contain stage labels that were correct when those documents were written. Current forward status is: **Selection Presentation G0-G7 sealed; G8 deferred.**
