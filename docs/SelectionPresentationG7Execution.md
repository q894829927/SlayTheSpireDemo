# Selection Presentation G7 Execution

Date: **2026-09-10**

Status:

```text
IN PROGRESS
G6 PREDECESSOR COMPLETE / VALIDATED / SEALED
COMPATIBILITY AUDIT COMPLETE
RUNTIME-BEHAVIOR CHANGES NOT AUTHORIZED IN G7 CLEANUP
G8 OUT OF SCOPE
```

G7 is a cleanup stage only. It must preserve the sealed G5/G6 production behavior, including exact SelectionArea ownership, G5 SingleRecord fallback, G6 transactional Group playback, `ConsumedPendingReducer`, exact cancellation/recovery and chronological reducer order.

## Audit result

The mandatory design targets were compared against current `main` before deleting anything.

### Already removed before G7

The coherent G4+G5 migration already removed the high-risk pre-G5 compatibility implementation from production Source:

```text
ConfirmedCardCenters / confirmed-center map
confirmed-position handoff as Selection continuity truth
Hand rebuild restoration of confirmed Selection transforms
Selection-subclass destination-specific Hand -> DrawPile animation shell
old Selection-specific destination ownership
```

Repository Source search on the G7 base finds `ConfirmedCardCenters` only in historical/design documentation and `UI/AGENTS.md`; there is no current runtime declaration or caller to delete.

The current Selection subclass owns only interaction/SelectionArea concerns and delegates committed destinations to `UBattleHUDCardTransitionWidget`.

### Current transition engine is not a cleanup target

The following are current required production mechanisms and must remain:

```text
FNativeCardTransitionInstance
ActiveNativeCardTransitions
SelectionArea -> Transition ownership transfer
Transition -> ConsumedPendingReducer transfer
transactional Group preflight / rollback
SelectionArea exact-object source resolver
exact-token transition cancellation and stale-ownership recovery
```

These implement sealed G5/G6 behavior rather than compatibility behavior.

### Retained older Native card infrastructure

`UBattleHUDWidget` still contains the older one-local-card machinery used by `CardPlayed`, `DrawPile -> Hand`, and `PlayArea -> destination`. It also contains direct Hand -> Discard/Exhaust handlers that are bypassed by the production `UBattleHUDCardTransitionWidget` subclass for supported outgoing Hand records.

G7 does **not** delete the shared old card machinery wholesale because it still owns required non-Selection Record behavior and recovery. Deleting it would be a migration/redesign rather than cleanup.

The direct base Hand -> Discard/Exhaust handlers are likewise retained unless a separate migration proves every direct `UBattleHUDWidget` caller/test no longer depends on that base behavior. G7 must not manufacture equivalence evidence by simply removing historical regression coverage.

### Proven dead/staging residue

The initial audit found one source-level compatibility residue that is provably dead:

```text
UBattleHUDSelectionWidget::FinishSharedHandToDrawPilePresentationForTesting
```

It is an unused G5-era alias for the generic `FinishNativeCardTransitionForTesting` helper. No source/test caller references the alias. The inherited generic test helper remains available, so deleting the alias changes no runtime or test capability.

Source comments that still describe the generic transition surface as a future/dormant G4 adapter may be normalized to the sealed G5/G6 model, but comments are not permission to alter behavior.

## Planned G7 code change

```text
1. delete the unused Selection-specific test alias;
2. normalize stale transition comments where touched;
3. do not remove current recovery hooks, ownership state or generic transition code;
4. do not perform optional Status reconciliation in this stage;
5. do not enter G8.
```

## Validation boundary

Because the intended runtime behavior is unchanged, G7 requires fresh affected compile/Automation evidence but does not invalidate the already accepted G6 visual contract unless a cleanup edit changes a real visible path.

Required after the source cleanup:

```text
[ ] UE 5.8 Development Editor build PASS
[ ] SlayTheSpireDemo.SelectionPresentation.G6 focused regression PASS
[ ] affected CardSelection.Presentation G5/G4 regression PASS
[ ] affected C0 multi-select regression PASS if shared Selection code changed materially
[ ] affected C1 DrawPileTop regression PASS if transition behavior changed materially
```

If the final G7 diff remains declaration/comment/test-only with no runtime visual behavior change, the sealed G6 Native PIE evidence remains valid under `docs/ValidationExecutionPolicy.md`; no fabricated new PIE claim will be made.

## Seal rule

G7 may be sealed only after the final diff is reviewed against this audit and the affected build/Automation gates pass. The seal must explicitly state that G8 early input / Presentation pipelining remains deferred and unimplemented.