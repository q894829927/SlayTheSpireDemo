# Selection Presentation G5 Execution

Date: **2026-09-09**

Status: **IMPLEMENTED / AUTOMATED GATES PASS / MANUAL PIE PENDING / UNSEALED**

User authorized G5 after G4's sequential compatibility path failed visual
acceptance. Base HEAD: `963adbd27cc161a09ea1ff68c7e479719b8331cb`, with the
uncommitted compilation/position repairs recorded in `docs/Validation.md`.
This is the coherent G4+G5 delivery allowed by Group design section 28; it
supersedes the G4-only execution requirement to seal the failed intermediate
visual implementation before G5. It does not claim that G4's visual gate passed.

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
are not summed. No PIE or simultaneous animation acceptance is claimed.

**AUTOMATED GATES:** bundled UE 5.8 project generation + Development Editor build;
focused CardSelection.Presentation and new G5 tests, plus directly affected G0
ownership/G1 correlation coverage. Verify selection/deselection, coherent surface
notifications, exact object retention, Confirm rejection/replacement, recorded
and direct receipts, zero destinations, missing metadata recovery, transition
decline/cancel and sequential consumption. Record actual scope/counts/results.

**MANUAL PIE — USER ACTION REQUIRED:** Native `L_BattleTest`, select/deselect/
reselect; Confirm multiple Exhaust candidates and observe second/third cards
remain in place while prior cards disappear (still sequential); Warcry select
and Confirm transfers that exact visible card to DrawPile, with played-card
cleanup afterward. No flashback, clipping, duplicate, ghost or stuck input.
G4+G5 stays unsealed until these visual results are provided.

Next stage after acceptance: G6 safe N-child simultaneous playback.
