# Codex Goal Checkpoint — Selection Presentation / Card Expansion

Last updated: **2026-09-10**

Use `main` as the working branch unless a later explicit branch decision supersedes it.

## Current resumable task — Fan Hand / Attack Targeting

HEAD `268e3006b8f04e924af6d09a020c45460b35c103`; local uncommitted changes.
Scope: user-requested fan Hand, enlarged hover cards and gray/red single-enemy
Attack arrow and viewport-wide mouse-right cancellation for ordinary selection; [execution/evidence](HandFanTargetingInteraction.md). G8 stays deferred.

Completed: UBattleHandFanPanel with ordered Canvas slots, stable hover regions and
raised-card painting; UBattleTargetingArrowWidget with private imported textures,
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
