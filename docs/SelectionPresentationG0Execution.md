# Selection Presentation G0 Execution Record

## Scope

This record tracks implementation of:

```text
G0-A — incremental HUD dirty propagation
G0-B — RuntimeId-keyed formal Hand reconciliation
G0-C — dormant Presentation ownership infrastructure
```

G0 is infrastructure only. Production Selection remains on the existing formal-Hand / `ConfirmedCardCenters` compatibility path until the later generic transition and ownership migration stages.

## Current status

```text
G0-A implementation: COMPLETE, validation pending
G0-B implementation: COMPLETE, validation pending
G0-C dormant infrastructure: COMPLETE, validation pending
G1 production work: BLOCKED until fresh G0 Build + focused Automation evidence
```

No `.uasset` or `.umap` change is part of G0.

## G0-A implemented contract

The ViewModel now publishes an exact native dirty descriptor for historical/read-facing HUD changes while retaining the payload-free Blueprint `OnChanged` compatibility event.

Native consumers receive the dirty value as delegate payload rather than reading a mutable global "last flags" value during callback dispatch. This prevents nested ViewModel publications from changing the dirty descriptor observed by a later native listener in the outer dispatch.

Dirty surfaces include:

```text
Hand
Combatants
Statuses
Energy
PileCounts
Input
Feedback
Intent
Terminal
PresentationAvailability
```

The generic Native reconciled HUD consumes those flags and does not treat unrelated Energy/Status/feedback changes as permission to rebuild Hand.

## G0-B implemented contract

`UBattleHUDReconciledWidget` is the Selection-agnostic Native reconciliation layer.

Formal Hand identity is:

```text
(BattleId, RuntimeId)
```

Within the same Battle:

```text
survivor RuntimeId → reuse exact UBattleCardWidget UObject
removed RuntimeId  → leave formal Hand
added RuntimeId    → create one new formal Widget
order              → reattach in exact frozen Hand order
```

A Battle replacement intentionally retires all prior formal card Widget identities even if a RuntimeId value is reused.

The keyed reconciler uses the same base `UBattleHUDWidget::HandleCardRequested` delegate as the fallback full-rebuild path. This avoids duplicate request handlers after malformed-state fallback followed by a later keyed reconcile.

Creation supports both PlayerController-owned production Widgets and a World-owned Native test/runtime shell.

## G0-C implemented contract

Transient card Presentation ownership is logically separate from `FPresentationStateSnapshot`.

Owner chain:

```text
Hand
→ SelectionArea
→ Transition
→ ConsumedPendingReducer
→ Done / implicit Hand after exact recovery
```

Lifecycle identity carries:

```text
BattleId
SelectionGeneration
SelectionBoundaryRevision
RuntimeId
Pending / Confirmed phase
CompletionWatermark
```

Interactive ownership generation is closed on Confirm or explicit lifecycle cancellation. A stale generation cannot continue mutating ownership after that edge.

A RuntimeId must exist in the currently displayed Hand before it may enter Pending SelectionArea ownership.

Confirm is fail-closed and freezes the exact current SelectionArea set. The supplied RuntimeId set must equal every Pending SelectionArea entry in that exact `(BattleId, SelectionGeneration, SelectionBoundaryRevision)` lifecycle. A subset, superset, duplicate, or malformed lifecycle shape is rejected without changing ownership or closing the generation.

Only these forward ownership transfers are legal after Confirm:

```text
SelectionArea → Transition
Transition → ConsumedPendingReducer
```

Backward or direct-to-Hand transfer through the transition API is rejected. Recovery to Hand is owned by reconciliation.

### Completion watermark

Supported modes:

```text
Unresolved
RecordedResolution
DirectStateRevision
```

Rules:

- request/resolver disappearance is never lifecycle completion;
- Direct completion must target a StateRevision strictly newer than the Selection boundary;
- an armed watermark is immutable except for idempotently re-arming the exact same value;
- exact recorded completion is represented through `MarkPresentationResolutionCompleted(BattleId, ResolutionId)`;
- normal historical snapshot copy does not reset ownership.

Reconciliation clears ownership immediately when the RuntimeId is absent from displayed Hand.

If the card is still in displayed Hand, an exact reached completion watermark is the final fail-safe for any Confirmed non-Hand owner, including `SelectionArea`, `Transition`, and `ConsumedPendingReducer`. This prevents timeout/collapse/recovery from leaving a permanent Hidden ghost slot.

### Ownership dirty channel

Ownership mutation has a native event independent of historical snapshot dirty publication.

The generic reconciled HUD listens to it and makes an explicit non-Hand formal slot:

```text
Visibility = Hidden
Enabled    = false
```

The child remains present at its historical Hand position. Clearing the explicit owner restores normal formal visibility/input without replacing the Widget object.

When one historical snapshot both changes displayed state and reconciles ownership, state is made coherent first, then historical/native dirty is published, then ownership dirty is published. All synchronous listeners therefore read final state rather than stale pre-reconciliation ownership.

### SelectionAreaHost

`UBattleHUDSelectionWidget` creates a runtime `SelectionAreaHost` separate from `HB_Hand` and `OV_PlayArea`. It is currently empty/dormant and production selected cards are not moved into it during G0.

The G5 production migration must preflight Host availability and must not switch visible ownership if the Host cannot be established. Persistent post-Confirm Z-order while real SelectionArea children exist remains a G5 acceptance item.

## Self-review fixes applied before validation

Self-review found and corrected these issues:

1. dirty flags were initially consumed only by the Selection subclass; moved to a generic reconciled Native HUD layer;
2. native dirty consumers initially relied on mutable `LastChangeFlags`; replaced with a payload delegate;
3. derived formal-Hand request delegates could coexist with the base fallback delegate; reduced to one base-owned handler;
4. RuntimeId Widget identity was initially not Battle-scoped; now `(BattleId, RuntimeId)` scoped;
5. Pending ownership initially accepted non-Hand RuntimeIds; now rejected;
6. Confirm initially left the interactive generation open; now closes it;
7. Direct watermark initially allowed the Selection boundary revision itself; now requires a strictly newer post-Confirm revision;
8. completion watermark could be overwritten across modes/values; now immutable/idempotent;
9. ownership transfer initially permitted insufficiently constrained transitions; now only the forward chain is legal;
10. completion recovery initially covered only SelectionArea owner; now exact lifecycle completion can recover any Confirmed non-Hand owner still present in Hand;
11. focused tests contained the old Transition-recovery expectation; updated to the fail-safe lifecycle contract;
12. Confirm initially accepted a subset of already Pending SelectionArea owners and silently restored omitted members; Confirm now requires exact lifecycle-set equality and rejects mismatches transactionally.

## Focused Automation source added

Current G0 test source covers:

```text
SlayTheSpireDemo.SelectionPresentation.G0.DirtyPropagation
SlayTheSpireDemo.SelectionPresentation.G0.OwnershipEventAndConfirm
SlayTheSpireDemo.SelectionPresentation.G0.CompletionWatermark
SlayTheSpireDemo.SelectionPresentation.G0.StaleAndTransition
SlayTheSpireDemo.SelectionPresentation.G0.PendingLifecycleCancel
SlayTheSpireDemo.SelectionPresentation.G0.ConfirmRequiresExactSet
SlayTheSpireDemo.SelectionPresentation.G0.HandIdentity
SlayTheSpireDemo.SelectionPresentation.G0.FormalSlotOwnership
```

Key assertions include:

- Energy/Status-only historical publication does not mark Hand dirty;
- same-Battle surviving RuntimeIds retain exact formal Widget object identity;
- RuntimeId identity does not cross Battle replacement;
- explicit non-Hand ownership keeps the formal structural child Hidden rather than Collapsed and disables input;
- ownership changes can update formal Hand without a new historical snapshot;
- Pending cancel releases all Pending owners and the active generation gate;
- Confirm rejects a partial RuntimeId set without mutating Pending owners or closing the generation, then accepts the exact retry;
- stale generation/boundary mutation is rejected;
- Direct and Recorded completion watermarks are exact and non-overwritable;
- only forward ownership transitions are accepted;
- final lifecycle completion recovers SelectionArea/Transition/Consumed ownership if the card still exists in Hand;
- authoritative Hand absence clears ownership independently of visual completion.

## Deliberately deferred integration

`MarkPresentationResolutionCompleted()` is the G0-C Controller-facing completion API, but production Controller completion/collapse/timeout wiring is deliberately deferred to the later playback/recovery hardening stage rather than modifying the Controller kernel twice.

Likewise, production Selection does not call the new ownership APIs during G0. That switch belongs after the generic transition source resolver can consume a SelectionArea-owned visual.

These are staged dependencies, not G0 validation claims.

## Validation state

At the time of this record:

```text
Build:      NOT RUN
Automation: NOT RUN
PIE/manual: NOT RUN
```

Do not enter G1 production implementation until a fresh UE build and the focused `SlayTheSpireDemo.SelectionPresentation.G0` Automation filter pass after the final G0 code changes.
