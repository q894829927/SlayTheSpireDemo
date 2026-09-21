# Codex Goal Checkpoint — Interior Portals

## Current P1A-3/P1A-4 implementation checkpoint — 2026-09-21

- P1A-4 implementation is committed in `787e507425131f03114794ea1ab3d91b5fdb1cc1`.
  Focused Unreal Insights instrumentation is committed in
  `5b6ad649d3e3d8977a79596132168d5067bdd0b2`.
- Queue-safe ViewState retirement on RequestedDepth reduction remains implemented
  for visible/offscreen/unlinked/viewport-unavailable paths, with detached
  dependent extensions, RHI-thread fence polling, bounded old/new coexistence,
  and synchronous Stop teardown only.
- UE 5.8 project generation and Development Editor build previously PASS.
  Focused CPU Automation remains 10/10 PASS. Actual D3D12 ownership evidence
  still verifies 8 -> 2 owned ViewStates, stable L0, distinct rebuilt IDs,
  rapid-reversal coexistence and zero Stop ownership.
- New user-captured Unreal Insights evidence closes the earlier attribution
  question around the approximately one-second rapid-expansion wall interval.
  On the actual Depth 1 -> 4 expansion frame, six rebuilt ViewStates cost 21 us
  total, six recursive extensions cost 25.9 us total, the complete
  `Portal_SubmitVisibleEndpoints` scope cost 591.4 us, `Portal_UpdateCapacity`
  cost 800 ns, and `Portal_PollRetirements` cost 100 ns.
- Stable rendering samples from the same manual session were approximately
  31.34 ms at RequestedDepth=1 and 145.78 ms at RequestedDepth=4 (~4.65x).
  Additional sustained depth-four samples were roughly 215-262 ms. The traces
  are dominated by repeated full scene rendering work rather than P1A-4 CPU
  lifetime management. Carry this forward as a P1B recursion-rendering
  performance baseline; do not reopen P1A-4 retirement merely to chase the
  steady-state Depth=4 GPU cost.
- P1A-4 transition-performance attribution for the CPU lifetime-management path
  is CLOSED. The user has now also confirmed the lifecycle-transition visuals
  are normal during 4 -> 1 -> 4, with no stale flash, prolonged blank aperture,
  recursion corruption or foreground-occlusion regression.
- The user clarified that the brief hitch while physically traversing a Portal
  is a pre-existing persistent issue that predates P1A-4. Treat it as a separate,
  currently unattributed traversal/performance issue rather than a reclaim
  regression; do not count it as a P1A-4 failure without focused evidence tying
  it to the lifetime path.
- **P1A-4 queue-safe runtime reclaim = COMPLETE / VALIDATED for its defined
  scope.**
- P1A-5 fallback/legacy Color Target Capacity Shrink is now implemented in
  `6404baf08f0fcfce0a3dd21ed1aa5fb94da541dd`. L1-L3 color targets above a
  reduced RequestedDepth retire with the same `FRetiringLayer` and RHI-thread
  fence that owns the old ViewState/publication; report schema v4 exposes active
  and retiring color counts, and Stop releases active fallback color targets.
- P1A-5 is **IMPLEMENTED / D3D12 CORE MATRIX PASS / REMAINING VALIDATION
  REQUIRED**, not yet sealed. User-provided fallback D3D12 dumps verify settled
  `4 -> 1 -> 4` color counts `4 -> 1 -> 4` on both endpoints, stable retained
  L0 identities, fresh rebuilt L1-L3 identities, no offscreen shrink at
  RequestedDepth=4, and Stop ownership of zero color/depth/ViewState targets.
- The user confirmed the UE 5.8 Development Editor build PASS,
  `SlayTheSpireDemo.Interior.Portals.FullFidelity` Automation PASS, and a
  deliberate rapid `4 -> 1 -> 4` reversal with no stale/blank/crash regression.
  Together with the D3D12 settled shrink/regrowth, offscreen invariance, and Stop
  teardown evidence, **P1A-5 Color Target Capacity Shrink = COMPLETE / VALIDATED**.
  Authority:
  [P1A-5 validation](InteriorPortalExperiment/InteriorPortalColorTargetCapacityValidation.md).
- Current active implementation step: **P1A-6 Depth Target Capacity Reclaim**.
  Preserve lazy depth-target creation; reclaim only out-of-budget old lifetimes
  after safe retirement. Then continue P1A-7 and the full P1A gate. Do not skip
  directly to broad physics. P1B remains the later stage for sustained
  depth-four rendering cost.
- Exact evidence and caveats:
  [capacity validation](InteriorPortalExperiment/InteriorPortalCapacityReclaimValidation.md).

## Historical resume update — user-confirmed visual acceptance

- Verified HEAD: `684ebae1c06215196c7bc73ef1c87a8d61030af9`.
- Repair committed in `9259f8e`; physical interaction design committed in
  `684ebae`. The earlier repair checkpoint below is historical.
- User confirmed `portal.FullFidelityPingPong 1` acceptance complete in this
  conversation. This is user-reported manual evidence; no fresh agent PIE,
  build or Automation was performed for this documentation update.
- Code default remains `0`; visual acceptance no longer blocks a separate
  change restoring the optimized default.
- Next implementation: P1A-4 queue-safe runtime reclaim, including the remaining
  P1A-3 depth-decrease retirement integration. Existing lazy allocation and
  lifetime identity do not prove the entire P1A-3 gate complete.
- Validate depth transitions `4 -> 1`, `1 -> 4`, and rapid `4 -> 1 -> 4`, stale
  publication protection, retirement completion and transition hitch/VRAM.
- Full-view depth-4 OOM remains an open resource limitation. Continue P1A-5/6
  capacity work and P1A acceptance before broad physical interaction work.
- Authority: `docs/PortalPerformanceVRAMP1Plan.md`; physical design is planned,
  not implemented. Documentation references and whitespace checked this turn.

## Historical Portal repair checkpoint — 2026-09-20

- Branch: `portal/full-fidelity-p1`; HEAD:
  `df09691ff277c055621a18602c8d93f1086f7f78`. No repair commit created.
- Current user-authorized task: repair the reproduced crop/slit display defect
  while retaining Ping-Pong. Dedicated evidence and remaining acceptance:
  [InteriorPortalCropDisplayRegression.md](InteriorPortalExperiment/InteriorPortalCropDisplayRegression.md).
- Completed in the working tree: full receiving-view pixel/NDC mapping for
  color aperture and depth candidate; actual primary-view depth extraction
  through the view uniform; paired color/depth publication guard; truthful
  extraction diagnostics; deterministic off-center/recursive crop regression.
- Build and focused Automation: PASS (9/9). Actual D3D12 PIE captures verify
  repaired far/mid/near output with Ping-Pong 0/1, optimized diagnostic matrix,
  and a temporary facing-pair fixture with four layers submitted/published.
  Exact evidence and the corrected initial shader signature failure are in
  the dedicated record. Do not rerun unchanged CPU gates solely to resume.
- Pending: final continuous-motion manual PIE acceptance. Ping-Pong default is
  temporarily `0`; use `portal.FullFidelityPingPong 1` before a fresh PIE for
  acceptance. The optimized implementation and TSR/Lumen quality are retained.
- Known validation limit: full-view depth-4 expansion encountered D3D12 OOM
  with the debug layer enabled; optimized four-layer fixture later ran without
  that layer. This does not close the separate performance/VRAM resource gate.
- Existing map modification preserved, SHA256
  `1F8CCDC4B8A5D82B8DB469D8F6F4D290F961FAABE5E17708B89706103ED26E5B`.
  PIE-only fixture transforms were restored; no map/asset saves performed.
- Next action: review the working-tree repair and complete the short manual
  motion/turn/traversal matrix in the dedicated record, then decide whether to
  restore the optimized default. Do not resume obsolete P1A-1 instructions.
  Performance work remains governed by `docs/PortalPerformanceVRAMP1Plan.md`
  and is outside this display repair.

## Historical Portal resume authority — 2026-09-16

Branch:

```text
portal/full-fidelity-p1
```

Current authorized Portal plan:

```text
docs/PortalPerformanceVRAMP1Plan.md
```

Current functional closure record:

```text
docs/InteriorPortalExperiment/InteriorPortalFullFidelityProductionRegression.md
```

Current Portal task:

```text
P1A-1 — Structured resource baseline and Dump instrumentation
```

Current status:

```text
SINGLE-PAIR FULLFIDELITY FUNCTIONAL GATE = COMPLETE / VALIDATED / SEALED
PORTAL PERFORMANCE / VRAM P1 = AUTHORIZED
P1A-0 DOCUMENTATION ALIGNMENT = COMPLETE
P1A-1 = NEXT IMPLEMENTATION TASK
```

Resume rule:

- Do **not** resume the historical STEP 1B.3 / CRP feasibility task below.
- Do **not** reopen accepted projection, aperture, depth, TSR, Lumen, composition or publication-retirement behavior without a reproduced regression.
- Start P1A-1 by improving structured diagnostics only; do not change resource allocation/reclaim semantics except where strictly required for truthful reporting.
- Report `RequestedDepth`, `VisibleDepth`, `EffectiveDepth`, exact attempted/submitted layer masks and resource ownership separately.
- Lifetime/state fields that do not exist yet must be reported as `INFERRED` or `NOT_IMPLEMENTED`; do not implement the P1A-2 state machine early merely to populate the report.

The historical checkpoint below is retained for engineering evidence only.

---

## HISTORICAL CHECKPOINT — STEP 1B.3 Main SceneColor composition boundary, 2026-09-14

Branch: `portal/full-fidelity-p1`. Current baseline for this delivery:
`81ffb09af1945ed8f695c934bcfacd67ab29f835` (`portal: add native exit clip diagnostics`).
The existing user-owned map change in
`Content/House/L_Interior_LivingKitchen.umap` remains unrelated and must stay
uncommitted.

STEP 1B.3 result: **PARTIAL**. `CustomRenderPassCompositionSpike` was added as
an explicit opt-in backend while preserving the default `SceneCapture`
fallback, `MainViewStencilSpike` and `CustomRenderPassSpike`. It submits the
existing real transformed one-layer CRP view, copies the external target
resource identity into the immutable request, and subscribes to
`ISceneViewExtension::SubscribeToPostProcessingPass(BeforeDOF)`. The callback
reads `FPostProcessMaterialInputs::SceneColor`, imports the CRP HDR target and
returns a new RDG SceneColor before player exposure/local exposure/color
grading/tonemap.

The project-side pre-tonemap composition boundary is therefore implemented,
but the spike uses an analytic ellipse from logical projected bounds. It does
not apply main SceneDepth comparison, public main stencil, true CRP scissor or
depth-continuous wall occlusion. GPU composition and visual parity remain
manual PIE/RenderDoc evidence, not Automation claims. No Engine source was
modified, and no SceneCapture exposure/ObliqueFallback/traversal/physics path
was changed.

Changed source includes `InteriorPortalRenderer.h/.cpp`,
`InteriorPortalSystem.h/.cpp`, `SlayTheSpireDemo.Build.cs`,
`SlayTheSpireDemo.uproject`, `Shaders/InteriorPortalComposition.usf` and
`InteriorPortalTests.cpp`; docs are updated in the current execution plan and
observed-issues log. The SceneCapture backend remains the default fallback.

Validation: bundled UE 5.8 project-file generation passed; Development Editor
build passed; focused `SlayTheSpireDemo.Interior.Portals` passed **17/17** in
`Saved/AutomationReports/PortalCompositionBoundary/index.json`. The first
Automation invocation was discarded because `-run=AutomationTests` requested a
nonexistent commandlet; the corrected `UnrealEditor-Cmd` invocation without
that flag passed. The module loads at `PostConfigInit` because UE5.8 rejects
project global shader registration after shader types initialize.

Next action: manual PIE on `/Game/House/L_Interior_LivingKitchen` with
`RendererBackend=CustomRenderPassCompositionSpike`, `RecursionDepth=1`, then
compare frontal/vertical/near/partial-offscreen and bright↔dark views. Check
parallax, ellipse edge, portal-outside contamination, support-wall/depth
occlusion, exposure/tone-map behavior and crossing frame pops. Record as
`MANUAL VISUAL ACCEPTANCE REQUIRED`; do not claim SUCCESS from 17/17. If the
visual path is useful, the next feasibility decision is the minimal
renderer-private depth-stencil/scissor hook, not a SceneCapture brightness
hack. Do not start Step 2 or Step 3, recursion > 1, full temporal/Lumen
fidelity or physics work from this checkpoint.

Rollback checkpoint, 2026-09-13 14:22: the uncommitted portal files were reconstructed from the session history immediately before the 14:22 MCP capture rather than using `git reset --hard`, which would have discarded the earlier portal and presentation work. The map-owned pair and generated assets were rebuilt and saved. UE 5.8's HDR RenderTarget contract was preserved with a Linear Color sampler, and stale disconnected material expressions were removed so `M_InteriorPortal` compiles instead of falling back to the default shader. Final MCP viewport evidence is `Saved/PortalRollback1422FinalViewport.png`; PIE was stopped after capture. The final editor viewport shows both animated portal rings with no checkerboard/default-material surface.

Post-rollback focused Automation was rediscovered and rerun through MCP: 8/8 passed in `Saved/PortalRollback1422Tests.json`.

Evidence caveat: discard `Saved/PortalWallStuckBaseline.json` and the initial `Saved/PortalWallStuckFixed.json`. UE Python unary vector negation modified a reused direction, invalidating those ad hoc labels. Corrected probe uses immutable scalar directions. Native Automation and the six-case replay remain valid. Full details and acceptance limits: `docs/InteriorPortalPlayerTraversal.md`; current defect authority: `docs/InteriorPortalObservedIssues.md`.

Next action: perform the remaining manual visual PIE matrix on `/Game/House/L_Interior_LivingKitchen` for both `FinalColorHDR` and `SceneColorLinear`: bright→dark, dark→bright, static, approach, crossing, slow camera rotation, recursion 1 and recursion >=2. Record screenshots/observations for exposure parity, clipping, Lumen and temporal behavior; automation and JSON diagnostics cannot substitute for this evidence. Do not begin Step 1B Stencil/MainView, Step 2 or Step 3. No Core or Full Physics seal is claimed. The implementation commit is `b89552a`; the remaining work is manual visual acceptance, not an uncommitted code continuation.

Tool navigation: MCP endpoint `http://127.0.0.1:8000/mcp`; `Saved/PortalMcp.ps1` handles SSE (reinitialize the session after editor restart). `Saved/PortalP34Bridge.py` runs through the UE Python plugin at editor startup and consumes `Saved/PortalP34Command.py`. Publish commands atomically via a temporary file and rename; partial writes can execute twice. Do not bypass MCP ProgrammaticToolset's sandbox. The verification PIE session was stopped through MCP; close the background editor before a future build if it is still running. The map and generated presentation assets are intentionally saved as part of this pass; no commit was made.

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
[IroncladCharacterAnimation.md](IroncladCharacterAnimation.md). Bundled project-file generation and Development Editor build passed (`Saved/Logs/IroncladCharacterAnimationProjectFiles.log`, `Saved/Logs/IroncladCharacterAnimationBuild.log`).
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
