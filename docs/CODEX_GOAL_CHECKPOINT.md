# Codex Goal Checkpoint — Selection Presentation / Card Expansion

Last updated: **2026-09-10**

Use `main` as the working branch unless a later explicit branch decision supersedes it.

## Current resumable task — Selection Presentation G7 validation

**Current active stage:**

```text
G7 — compatibility/dead-path cleanup
     audit COMPLETE
     cleanup IMPLEMENTED
     Development Editor build PENDING
     focused regression Automation PENDING
     NOT SEALED
     G8 remains out of scope
```

Do **not** resume from G1–G6. Those stages are complete and sealed as described below.

### Current Selection Presentation status

```text
G0 — COMPLETE / VALIDATED / SEALED
     incremental ViewModel/HUD dirty propagation
     RuntimeId-keyed formal Hand reconcile
     dormant ownership lifecycle foundation

G1 — COMPLETE / VALIDATED / SEALED
     exact Selection outcome correlation
     writer-scoped optional Group metadata
     canonical selected-identity manifest

G2 — COMPLETE / VALIDATED / SEALED
     Controller semantic Group discovery
     reducer dry-run / interference validation

G3 — COMPLETE / VALIDATED / SEALED
     Record-or-Group playback-unit hardening
     exact completion and scoped recovery

G4 — generic SingleRecord card-transition engine implemented
     isolated G4-only compatibility-position PIE = HISTORICAL FAIL
     do not restore that compatibility path as production authority

G5 — COMPLETE / VALIDATED / SEALED
     persistent production SelectionArea ownership
     exact same-object SingleRecord consumption
     transactional Confirm / outcome handling
     recorded/direct completion + recovery

G4+G5 coherent production delivery
   — COMPLETE / VALIDATED / SEALED

G6 — COMPLETE / VALIDATED / SEALED
     transactional N-child Group visual preflight
     atomic SelectionArea → Transition cohort transfer
     Transition → ConsumedPendingReducer lifecycle
     visual co-presentation with serial authoritative reducer chronology
     exact G5 sequential fallback retained
     Development Editor build PASS
     focused G6 Automation 4/4 PASS
     Native L_BattleTest simultaneous visual PIE PASS / user confirmed 2026-09-10

G7 — CLEANUP IMPLEMENTED / VALIDATION PENDING / NOT SEALED
     compatibility audit complete
     no runtime behavior branch changed
     unused Selection-specific test alias removed
     stale transition staging comments normalized

G8 — SEPARATE DEFERRED INITIATIVE / early input + Presentation pipelining
```

## G6 final authority

Dedicated execution/seal record:

```text
docs/SelectionPresentationG6Execution.md
```

Production implementation landed through the focused G6 commit chain. Key runtime commits include:

```text
cedd3581779f154f3fc228290e9f3c45a7452250  feat(g6): add atomic card ownership transfer surface
55ba87200a2c03c5bf98b55df4143391c1d77e6e  feat(g6): implement atomic ownership batch transfer
76b07d6b0d6cf859a63ee363385da6b78f5ca7c4  feat(g6): implement transactional native group transitions
cd13db609546bf4061c8ab197cd9df78190cdf75   feat(g6): add production group activation state
5c2920700046d2b19eaee7dbc612c02908017773   feat(g6): activate semantic groups without reducer reordering
85f62cb1e0580f0b04a145a0997c1d92275b15ec   feat(g6): activate groups at selection playback boundary
f5f1a3c863a3f6054d7680cf58209b9e93092c11   fix(g6): complete groups through chronological reducer path
e9b566a7ca0db80e11e0cba70542af25480ba416   fix(g6): match offered records by immutable identity
```

Focused tests landed through:

```text
c12455742ba5d41eda0a2bca09d7f045265ea0ab
test(g6): cover parallel playback contracts
```

Final G6 evidence:

```text
[x] UE 5.8 SlayTheSpireDemoEditor Win64 Development build PASS
[x] SlayTheSpireDemo.SelectionPresentation.G6 focused Automation 4/4 PASS
[x] batch ownership all-or-nothing coverage
[x] tracked Group-upgrade rollback coverage
[x] ConsumedPendingReducer lifecycle coverage
[x] parallel visual / serial reducer chronology coverage
[x] Group decline → G5 sequential fallback coverage
[x] Native L_BattleTest Player multi-select N>1 PIE PASS
[x] selected destination animations visibly begin together
[x] exact SelectionArea starting positions preserved
[x] no Hand flashback
[x] no duplicate
[x] no ghost
[x] no clipping
[x] correct destination/pile result
[x] input restores normally and later selection works
[x] ordinary single-selection / Warcry path remains correct
```

This evidence seals G6. Headless Automation established protocol/control-flow behavior; the user-confirmed production Native PIE gate established the real simultaneous visual behavior.

## G7 audit and implementation authority

Dedicated execution record:

```text
docs/SelectionPresentationG7Execution.md
```

The G7 audit established that the high-risk compatibility targets named by the original plan had already been removed during the coherent G4+G5 migration:

```text
ConfirmedCardCenters / confirmed-center runtime map
confirmed-position restoration as Selection continuity
Selection-subclass destination-specific animation ownership
old Selection-specific Hand -> DrawPile shell
```

Current Source search contains no runtime `ConfirmedCardCenters` declaration/caller. The current per-child transition engine, SelectionArea source resolver, ownership lifecycle and exact recovery paths are production mechanisms and were explicitly retained.

The older `UBattleHUDWidget` one-local-card infrastructure was also retained because it still owns required `CardPlayed`, Draw and PlayArea cleanup behavior. G7 does not expand into a migration of those unrelated active Record paths.

The only source-level dead compatibility surface proven by the audit was an unused Selection-specific test alias for the already-generic transition test helper. Cleanup landed in:

```text
114f6e60a375d7c82548541f3088e66dad5d7296
cleanup(g7): remove obsolete selection test alias

4a3cff000b7e845aa4b5709222b9aebfebccbe86
cleanup(g7): normalize sealed transition comments
```

Final G7 Source diff is limited to:

```text
Source/SlayTheSpireDemo/UI/BattleHUDSelectionWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDCardTransitionWidget.h
```

No `.cpp` runtime branch, animation parameter, ownership mutation, Controller/reducer behavior, Gameplay rule, asset or map changed.

### Remaining G7 validation

Run fresh affected evidence:

```text
[ ] UE 5.8 SlayTheSpireDemoEditor Win64 Development build
[ ] SlayTheSpireDemo.SelectionPresentation.G6
[ ] SlayTheSpireDemo.CardSelection.Presentation
```

If these pass, G7 can be sealed without a new PIE run because the final cleanup is declaration/comment-only and does not invalidate the sealed G6 visual behavior. If a failure reveals an actual affected C0/C1 contract, expand validation only to that implicated scope under `docs/ValidationExecutionPolicy.md`.

G7 must not enable new overlapping input, cross-resolution pipelining or cosmetic NonBlocking semantics. Those belong only to G8.

## Current Card Expansion status

Card Expansion remains a separate initiative from Selection Presentation G7.

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

Wave 1C-C1 — Hand → DrawPileTop / Warcry capability
→ IMPLEMENTED
→ MERGED TO main by PR #18
→ merge commit ffbc164905a875bea5c9ab3dfe0a07df5068b8cc
→ Development Editor build PASS recorded
→ Wave1CC1.DrawPileTop focused Automation 7/7 PASS recorded
→ shared Selection Presentation visual path user-accepted
→ FINAL STANDALONE C1 USER SEAL RECORD NOT PRESENT

Former C1 True Grit plan
→ SUPERSEDED / NOT ACTIVE
→ True Grit remains a future thin content consumer of existing generalized primitives
```

Do not infer a standalone C1 `COMPLETE / VALIDATED / SEALED` state from PR #18 or shared Presentation evidence.

This checkpoint does **not** authorize a new Card Expansion slice. Wave 1D Reactive Exhaust Powers, production True Grit content, Card Trigger Source Expansion, multi-enemy work, Exhume and other capability waves require their own explicit scope/authority.

## Current authority chain

Selection Presentation:

```text
docs/CardSelectionPresentationConstraints.md
docs/SelectionPresentationGroupDesign.md
docs/SelectionPresentationGroupImplementationPlan.md
docs/SelectionPresentationG0Execution.md
docs/SelectionPresentationG1Execution.md
docs/SelectionPresentationG2Execution.md
docs/SelectionPresentationG3Execution.md
docs/SelectionPresentationG4Execution.md
docs/SelectionPresentationG5Execution.md
docs/SelectionPresentationG6Execution.md
docs/SelectionPresentationG7Execution.md
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

Validation policy:

```text
docs/ValidationExecutionPolicy.md
```

## Resume instruction

When work resumes, read this checkpoint, `docs/SelectionPresentationG7Execution.md`, `docs/SelectionPresentationG6Execution.md`, `docs/SelectionPresentationGroupImplementationPlan.md`, `docs/CardSelectionPresentationConstraints.md`, and the directory-level `AGENTS.md` files for any source area to be changed.

The immediate next action is the **G7 build + focused regression validation gate**. Do not begin additional cleanup deletion unless a new concrete dead path is independently proven. Do not begin G8.

Historical execution files may contain phrases such as “next active slice” that were correct when those stages were sealed. Treat those as historical sequencing, not current project status. Current forward status is this checkpoint.
