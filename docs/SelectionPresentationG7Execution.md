# Selection Presentation G7 Execution

Date: **2026-09-10**

Status:

```text
CLEANUP IMPLEMENTED /
COMPATIBILITY AUDIT COMPLETE /
DEVELOPMENT EDITOR BUILD PENDING /
FOCUSED REGRESSION AUTOMATION PENDING /
NOT SEALED /
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

The audit found one source-level compatibility residue that is provably dead:

```text
UBattleHUDSelectionWidget::FinishSharedHandToDrawPilePresentationForTesting
```

It was an unused G5-era alias for the generic `FinishNativeCardTransitionForTesting` helper. No source/test caller referenced the alias. The inherited generic test helper remains available, so deleting the alias changes no runtime or test capability.

Source comments that still described the generic transition surface as a future/dormant G4 adapter were also normalized to the sealed G5/G6 model.

## Implemented G7 cleanup

```text
114f6e60a375d7c82548541f3088e66dad5d7296
cleanup(g7): remove obsolete selection test alias

4a3cff000b7e845aa4b5709222b9aebfebccbe86
cleanup(g7): normalize sealed transition comments
```

The final Source cleanup changes only:

```text
Source/SlayTheSpireDemo/UI/BattleHUDSelectionWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDCardTransitionWidget.h
```

No `.cpp` runtime control flow, animation parameter, ownership mutation, Controller sequencing, reducer behavior, Gameplay rule, asset or map was changed.

Optional Status reconciliation is intentionally not included. G8 is intentionally not included.

## Validation boundary

Because the final runtime behavior is unchanged, G7 requires fresh affected compile/Automation evidence but does not invalidate the already accepted G6 visual contract.

Required after the source cleanup:

```text
[ ] UE 5.8 Development Editor build PASS
[ ] SlayTheSpireDemo.SelectionPresentation.G6 focused regression PASS
[ ] SlayTheSpireDemo.CardSelection.Presentation focused regression PASS
```

The C0/C1 implementation paths were not changed by the final G7 diff. Their earlier sealed/focused evidence remains valid unless the fresh shared Selection regression exposes a concrete failure implicating those contracts.

If the build and focused regressions pass, no fresh PIE is required for this declaration/comment-only runtime-neutral cleanup under `docs/ValidationExecutionPolicy.md`; the sealed G6 Native PIE evidence remains valid.

## Seal rule

G7 may be sealed only after the build and focused regressions above pass. The seal must explicitly state that G8 early input / Presentation pipelining remains deferred and unimplemented.