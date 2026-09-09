# Codex Goal Checkpoint — Selection Presentation / Card Expansion

Last updated: **2026-09-09**

Repository state verified through documentation sync commit:

```text
73a48d8da68de32316efbceda1812200c2bae6de
docs(validation): record G4+G5 manual acceptance
```

This checkpoint's own commit is newer and documentation-only. Use `main` as the working branch unless a later explicit branch decision supersedes it.

## Current resumable task — Selection Presentation G6

**Next active stage:**

```text
G6 — N-child Selection Presentation Group playback
     + transactional parallel visual preflight
     + atomic SelectionArea → Transition transfer
     + ConsumedPendingReducer lifecycle
     + exact sequential G5 fallback
```

Do **not** resume from G1, G2, G4 or G5. Those stages are already implemented and sealed as described below.

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
     visible Group playback still disabled

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

G6 — NEXT ACTIVE / NOT IMPLEMENTED
G7 — AFTER G6 / cleanup only after equivalence is proven
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

This acceptance applies to the coherent G4+G5 production path. It does **not** retroactively convert the failed isolated G4 compatibility experiment into a pass, and it does **not** claim G6 simultaneous animation acceptance.

## G6 implementation contract

G6 changes visible concurrency only. It MUST preserve:

```text
Gameplay mutation order
BattleEvent / trigger order
PresentationSequence order
chronological reducer order
```

The intended safe flow is:

```text
validated explicit Selection Presentation Group A/B/C
→ visual preflight all children without durable ownership mutation
→ if any child fails: rollback all preparation, transfer zero owners, use G5 sequential fallback
→ if all children accept: atomically transfer A/B/C SelectionArea → Transition
→ begin N destination animations in the same Native tick
→ visually finished future members become ConsumedPendingReducer
→ Controller resumes chronological reducer from the leader cursor
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

### G6 validation boundary

Fresh affected evidence is required because G6 changes production visible playback:

```text
Development Editor build
focused Selection Presentation / Group Automation
transactional prepare-failure coverage
same-tick N-child begin coverage
ConsumedPendingReducer / chronological reducer coverage
sequential fallback regression coverage
C0 multi-select regressions affected by shared Selection changes
C1 DrawPileTop / Warcry regressions affected by shared transition changes
Native L_BattleTest simultaneous/no-flash PIE
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

When work resumes, read the current status blocks in this checkpoint, `docs/SelectionPresentationGroupImplementationPlan.md`, `docs/CardSelectionPresentationConstraints.md`, and the G5 execution record. Then begin from the **G6 N-child Group playback boundary**.

Historical execution files may still contain phrases such as “next active slice” that were correct at the time that individual stage was sealed. Treat those as historical sequencing inside that execution record, not as current project status. Current forward status is the checkpoint above.
