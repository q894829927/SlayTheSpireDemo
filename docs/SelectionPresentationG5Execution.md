# Selection Presentation G5 Execution

Date: **2026-09-09**

Status: **COMPLETE / VALIDATED / SEALED**

User authorized G5 after G4's sequential compatibility path failed visual
acceptance. Base HEAD: `963adbd27cc161a09ea1ff68c7e479719b8331cb`, with the
uncommitted compilation/position repairs recorded in `docs/Validation.md`.
This is the coherent G4+G5 delivery allowed by Group design section 28; it
supersedes the G4-only execution requirement to seal the failed intermediate
visual implementation before G5. It does not claim that G4's historical G4-only
visual gate passed. The coherent G4+G5 production path is the accepted path.

Final implementation commit before manual acceptance:

```text
c7f1a799708dbce12712f4c418b004ac627d9b03
g4+g5完成
```

## Scope and ordering

1. Activate exact Pending/Confirmed SelectionArea ownership using the existing
   G0 lifecycle and G1 outcome receipts. Keep formal Hand structural slots Hidden.
2. Prepare Host and visible copies before ownership mutation; commit all affected
   surfaces before external notifications. Keep exact visual objects alive across
   Hand reconciliation and Confirm. Pending visual clicks can deselect.
3. Make Confirm transactional across synchronous submit/outcome/state callbacks;
   rejection with the same boundary preserves Pending, replacement/failure clears
   only the old lifecycle. Arm exact recorded/direct completion, including zero
   destination records; missing correlation has explicit UI-only recovery.
4. Consume the same SelectionArea object through G4 SingleRecord. Leave other
   selected objects stationary. Decline, skip, missing destination, direct mode,
   battle/HUD replacement and reached watermarks must not leave ghosts.
5. Retire the failed Hand-position compatibility code and its superseded tests.

No G6 simultaneous/group playback, Gameplay rule changes, asset edits, Legacy
runtime references, G8 input overlap or broad G7 cleanup is included.

## Implemented behavior

The production HUD creates a persistent full-Canvas Overlay above normal battle
surfaces, preflights its geometry before selecting, and keeps selected card copies
GC-tracked. Pending layout follows frozen Hand order; Confirm stops repositioning
these objects. Exact source resolution uses the stable Host and current transform,
not stale child geometry from a preceding layout frame.

The ViewModel buffers snapshots/read edges/receipts while submission is in
progress, then commits the existing irreversible ownership Confirm only after
acceptance. The Controller queues synchronous deliveries until that transaction
returns. Receipt processing precedes playback regardless of delegate order.
Direct Ready edges resolve exact receipts; missing correlation at a newer Ready
edge releases the stale visual scope and exposes UI-only unavailable state.

A single Native surface synchronization callback runs before public multicast
observers. It retires old visible objects before restoring formal Hand widgets.
G4 retains GC ownership while taking the exact source object; stale recovery
releases its exact transition token before formal Hand is restored.

The obsolete Selected-Hand position compensation, confirmed-center map and
destination-specific compatibility shell were removed because G5 replaces their
only production caller. Other CardPlayed/draw/PlayArea adapters remain intact.

## Validation

Standard UE 5.8 project generation and Development Editor build PASS; final
logs: `Saved/Logs/G5RecoveryProjectFiles.log`, `Saved/Logs/G5RecoveryBuild.log`.
Initial focused foundation/selection run: 39/39 PASS. Expanded closure exposed
a missing test-effect preview implementation (process aborted), then one real
missing-correlation UI-state overwrite (9/10 passed). Both were corrected.
Final affected rerun: G5 prefix plus G0.FormalSlotOwnership **6/6 PASS**, no
warnings/failures/skips, process exit 0; `Saved/AutomationReports/G5Recovery/index.json`.
Other passing scope remains recorded in `docs/Validation.md`; overlapping runs
are not summed.

**AUTOMATED GATES: PASS.** Bundled UE 5.8 project generation + Development Editor
build and focused CardSelection.Presentation / G5 coverage passed. Covered
selection/deselection, coherent surface notifications, exact object retention,
Confirm rejection/replacement, recorded/direct receipts, zero destinations,
missing metadata recovery, transition decline/cancel and sequential consumption.

### Manual PIE acceptance — PASS

User-confirmed on **2026-09-09** using the production Native battle flow.
The coherent G4+G5 path passed the required visual gate:

```text
[x] Selection / deselection / reselection behaves correctly
[x] Multiple selected Exhaust candidates remain visually stable while waiting
[x] Sequential consumption starts from each card's actual displayed position
[x] Warcry selection transfers the exact visible selected card toward DrawPile
[x] no flashback
[x] no duplicate visual
[x] no ghost visual
[x] no clipping
[x] no stuck input
```

This acceptance applies to the coherent G4+G5 production path. The earlier
G4-only compatibility implementation remains a historical failed intermediate
and is not retroactively declared visually accepted.

## Seal

```text
G4+G5 coherent delivery: COMPLETE / VALIDATED / SEALED
Production SelectionArea ownership: ACCEPTED
Generic sequential SelectionArea -> Transition consumption: ACCEPTED
G6 simultaneous Group playback: NOT IMPLEMENTED
```

No additional G4/G5 rerun is required unless a later edit invalidates this
evidence or a concrete regression directly implicates the sealed contracts.

Next active stage: **G6 — safe N-child simultaneous Group playback +
ConsumedPendingReducer child lifecycle**.
