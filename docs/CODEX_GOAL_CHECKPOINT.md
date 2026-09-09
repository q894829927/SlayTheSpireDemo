# Codex Goal Checkpoint — Selection Presentation / Card Expansion

Last updated: **2026-09-10**

Repository state verified through G6 implementation/test and execution-record commits. Current `main` includes:

```text
c12455742ba5d41eda0a2bca09d7f045265ea0ab
test(g6): cover parallel playback contracts

531d90b858ea74823af2e2a58d08dec2d71cd6df
修改文档状态

3b9098361cd1c820f558ac8cb4b1f4847def5fc5
docs(g6): record build and automation gates
```

Use `main` as the working branch unless a later explicit branch decision supersedes it.

## Current resumable task — Selection Presentation G6 Native PIE gate

**Current active stage:**

```text
G6 — N-child Selection Presentation Group playback
     IMPLEMENTED
     Development Editor build PASS
     focused G6 Automation 4/4 PASS
     Native L_BattleTest simultaneous visual PIE PENDING
     NOT SEALED
```

Do **not** resume from G1, G2, G4 or G5. Those stages are already implemented and sealed as described below. Do not start G7 cleanup until the G6 Native visual gate passes.

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

G6 — IMPLEMENTED / BUILD PASS / FOCUSED AUTOMATION 4/4 PASS
     N-child transactional Group playback activated
     ConsumedPendingReducer lifecycle activated
     exact G5 sequential fallback retained
     NATIVE PIE PENDING / NOT SEALED

G7 — BLOCKED UNTIL G6 PIE + SEAL / cleanup only after equivalence is proven
G8 — SEPARATE DEFERRED INITIATIVE / early input + Presentation pipelining
```

### G4+G5 implementation and acceptance authority

Implementation commit before final manual acceptance:

```text
c7f1a799708dbce12712f4c418b004ac627d9b03
g4+g5完成
```

Dedicated records:

```text
docs/SelectionPresentationG4Execution.md
docs/SelectionPresentationG5Execution.md
docs/Validation.md
```

Automated evidence retained in those records includes standard UE 5.8 project generation, Development Editor build, the initial broader foundation/selection run, correction of the missing-correlation recovery defect, and the final affected G5 + G0.FormalSlotOwnership **6/6 PASS** rerun. Do not arithmetically combine overlapping Automation runs.

**Manual Native `L_BattleTest` PIE gate: PASS / user confirmed 2026-09-09.**

```text
[x] select / deselect / reselect works
[x] multiple selected Exhaust cards remain at their own displayed positions while waiting
[x] each sequential transition consumes the exact visible SelectionArea card
[x] Warcry selected card transfers from the exact visible object toward DrawPile
[x] no flashback
[x] no duplicate
[x] no ghost
[x] no clipping
[x] no stuck input
```

This acceptance applies to the coherent G4+G5 production path. It does **not** retroactively convert the failed isolated G4 compatibility experiment into a pass, and it does **not** by itself establish G6 simultaneous animation acceptance.

## G6 implementation and current validation authority

Dedicated execution record:

```text
docs/SelectionPresentationG6Execution.md
```

Production implementation landed through the focused G6 commit chain beginning with atomic ownership transfer and ending with immutable offered-record matching. The key final runtime fixes are:

```text
f5f1a3c863a3f6054d7680cf58209b9e93092c11
fix(g6): complete groups through chronological reducer path

e9b566a7ca0db80e11e0cba70542af25480ba416
fix(g6): match offered records by immutable identity
```

A pre-existing incomplete-type include dependency exposed by the changed Unity compilation layout was fixed in:

```text
e7f722ca666a96b08258b9a59ef516cffc1b903f
fix(presentation): include record writer definition
```

The focused G6 test inventory landed through:

```text
c12455742ba5d41eda0a2bca09d7f045265ea0ab
test(g6): cover parallel playback contracts
```

User-confirmed automated state:

```text
[x] UE 5.8 SlayTheSpireDemoEditor Win64 Development build PASS
[x] SlayTheSpireDemo.SelectionPresentation.G6 focused Automation PASS
[x] G6 focused inventory = 4/4 PASS
```

Current G6 focused tests:

```text
SlayTheSpireDemo.SelectionPresentation.G6.BatchOwnershipAtomicity
SlayTheSpireDemo.SelectionPresentation.G6.TrackedUpgradeRollback
SlayTheSpireDemo.SelectionPresentation.G6.ParallelVisualSerialReducer
SlayTheSpireDemo.SelectionPresentation.G6.GroupDeclineSequentialFallback
```

These gates establish protocol/control-flow behavior, not real Slate-frame simultaneity.

## G6 implementation contract

G6 changes visible concurrency only. It MUST preserve:

```text
Gameplay mutation order
BattleEvent / trigger order
PresentationSequence order
chronological reducer order
```

The implemented safe flow is:

```text
validated explicit Selection Presentation Group A/B/C
→ visual preflight all children without durable ownership mutation
→ if any child fails: rollback all preparation, transfer zero owners, use G5 sequential fallback
→ if all children accept: atomically transfer A/B/C SelectionArea → Transition
→ begin N destination animations from the accepted cohort
→ visual completion atomically transfers A/B/C Transition → ConsumedPendingReducer
→ Controller reduces only the leader immediately
→ interleaved records retain chronological order
→ already-visually-presented future member records reduce later without replay
→ displayed-state ownership reconciliation clears exact consumed entries
```

Sequential fallback remains a first-class correctness path:

```text
A/B/C remain SelectionArea(Confirmed)
→ A SingleRecord
→ B/C remain stationary and SelectionArea-owned
→ B SingleRecord
→ C SingleRecord
```

No confirmed-position reconstruction, Hand flashback, duplicate visible owner or Effect/CardId-specific animation branch is permitted.

### Remaining G6 validation boundary

Completed automated gates:

```text
[x] Development Editor build
[x] focused Selection Presentation / Group Automation
[x] transactional ownership/failure coverage
[x] tracked-unit rollback coverage
[x] ConsumedPendingReducer / chronological reducer coverage
[x] sequential fallback regression coverage
```

Still required before G6 seal:

```text
[ ] Native L_BattleTest Player multi-select with N > 1
[ ] selected destination animations visibly begin together
[ ] no Hand flashback / duplicate / ghost
[ ] exact SelectionArea starting positions remain correct
[ ] no clipping introduced by simultaneous playback
[ ] correct final pile/destination state
[ ] input restores normally and a later selection still works
[ ] one ordinary single-selection path such as Warcry remains correct
```

Follow `docs/ValidationExecutionPolicy.md`: reuse unaffected sealed evidence; do not blindly rerun every historical suite.

G6 must not include broad G7 cleanup or G8 cross-resolution early-input behavior.

## Current Card Expansion status

Card Expansion is a separate initiative from the G6 implementation authorization. Current durable state:

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
→ shared later G4+G5 SelectionArea→DrawPile visual path user-accepted
→ FINAL STANDALONE C1 USER SEAL RECORD NOT PRESENT

Former C1 True Grit plan
→ SUPERSEDED / NOT ACTIVE
→ True Grit remains a future thin content consumer of existing generalized primitives
```

Do not describe C0 as active/unsealed. Do not describe C1 as not started. Conversely, do not infer a standalone C1 `COMPLETE / VALIDATED / SEALED` state merely from PR #18 merge and shared later Presentation evidence.

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

When work resumes now, read `docs/SelectionPresentationG6Execution.md`, this checkpoint, `docs/SelectionPresentationGroupImplementationPlan.md` and `docs/CardSelectionPresentationConstraints.md`. The immediate next action is the **production Native `L_BattleTest` G6 simultaneous visual PIE gate**.

If PIE passes, record the acceptance in the G6 execution record and current status authorities, update `docs/Validation.md`, seal G6, and only then make **G7 cleanup** the next active stage.

If PIE fails, preserve the failure as evidence and repair G6 while retaining the sealed G5 sequential fallback. Do not start G7 or G8.

Historical execution files may still contain phrases such as “next active slice” that were correct at the time that individual stage was sealed. Treat those as historical sequencing inside that execution record, not as current project status. Current forward status is the checkpoint above.
