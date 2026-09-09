# Battle UI Rules

Applies to `Source/SlayTheSpireDemo/UI/**`.

Consult documents by changed contract: `docs/Phase6UIA3CardFacePreviewAmendment.md` for card-face Preview; `docs/Phase6UIA2EImplementation.md` for historical playback; `docs/CardSelectionPresentationConstraints.md` for Selection; `docs/SelectionPresentationGroupDesign.md` and its implementation plan for ownership/group redesign. These are targeted references, not an all-files reading prerequisite.

Selection ownership/group rules below define the target design. Staged activation and acceptance remain governed by the implementation plan; they do not imply that all stages already exist or are authorized.

## Legacy UI Preservation

Use the Native battle UI only. Root `AGENTS.md` and `docs/LegacyUIPreservationPolicy.md` protect the retained Legacy assets: no new runtime/test use or parity dual-writing; production dependency count stays `0`; destructive removal and runtime recovery require separate explicit authorization.

## Authority Boundary

UI never owns Gameplay truth. Normal UI uses formal Query/Request APIs and must not directly construct or enqueue authoritative `BattleAction` objects.

Query results are advisory. Request APIs revalidate authoritative current state. `AcceptedForResolution` means accepted into Gameplay resolution, not that effects or playback are complete.

Gameplay card zones and Presentation visual ownership are separate concepts. UI MUST NOT create a Gameplay `SelectionZone` merely to model selected-card visuals.

## ViewModel

The ViewModel stores frozen player-facing display state, transient Presentation/input state and latest-only weak runtime bindings used to submit current Requests. It is not a second Gameplay model.

Historical display comes only from `FPresentationStateSnapshot` plus active immutable Presentation facts. It must not query mutable Gameplay objects or advance ahead of currently authorized Presentation chronology.

Runtime input bindings are not historical state. Refresh them only after Presentation catches up to the newest matching `(BattleId, StateRevision)`.

When Presentation is enabled, `OnReadStateReady` must not bypass Presenter/Controller and directly apply live state to the HUD ViewModel.

### Incremental HUD reconciliation

The existing generic `OnChanged → RefreshHUDFromViewModel()` full-refresh behavior is legacy architecture to be replaced during the Selection Presentation redesign. New work MUST NOT depend on "every historical change rebuilds every HUD surface" as a correctness mechanism.

Target behavior:

```text
old displayed state + new displayed state
→ exact dirty/change-set
→ update only affected HUD surfaces
```

A Damage/Energy/Status/feedback/input change that does not alter Hand structure must not recreate formal Hand Widgets merely because the ViewModel published a new historical snapshot.

`OnPreviewChanged` remains transient Preview-only and must never trigger formal Hand rebuilding.

Historical snapshots remain complete; incremental UI reconciliation does not make the ViewModel partial or mutable-Gameplay-driven.

## Formal Hand Identity and Reconciliation

Formal Hand presentation preserves frozen Hand ordering and historical slot/index semantics while existing index-based contracts remain.

Hand Widget identity should be RuntimeId-keyed and surviving cards should be reconciled/reused rather than destroyed through unconditional `HB_Hand->ClearChildren()` + recreate-all behavior.

Required distinction:

```text
Hand index
→ historical snapshot/order validation

RuntimeId
→ exact visual/presentation identity
```

Do not use array index as the primary way to locate the current visible card when Presentation ownership may place that RuntimeId outside the Hand surface.

## Card Presentation Ownership

The authoritative selected-card continuity model is Presentation ownership:

```text
Hand
→ SelectionArea
→ Transition
→ ConsumedPendingReducer
→ Done
```

This is transient Presentation state, not Gameplay membership.

At most one visible Presentation owner exists for an exact RuntimeId at a time.

Behavior must be scoped by lifecycle identity equivalent to:

```text
BattleId
SelectionGeneration / selection-lifecycle identity
selection boundary/revision
RuntimeId
owner
Pending vs Confirmed phase
completion watermark
```

A stale prior selection/battle callback must not mutate newer ownership merely because RuntimeId matches.

### Confirmed lifecycle completion watermark

A Confirmed lifecycle MUST have a runtime-comparable completion watermark. Clearing the pending request/resolver is **not** sufficient, because normal confirmation clears the request before its continuation Presentation is displayed.

Behavior must be equivalent to one of these modes:

```text
Recorded Presentation mode
→ watermark = owning/continuation ResolutionId
→ reached only when that Resolution is Presentation-complete,
  collapsed/reconciled to its FinalSnapshot, or otherwise formally completed

Direct/no-history mode
→ watermark = authoritative post-confirm StateRevision/baseline edge
→ reached only when that post-confirm baseline is the displayed state
```

The exact storage type may be a tagged/optional struct, but it must preserve `(BattleId, SelectionGeneration, boundary/revision)` scope and distinguish recorded-resolution completion from direct-state completion.

A selected RuntimeId may be cleared earlier if the displayed Hand already proves it was consumed. The watermark is required specifically for the fallback case where the RuntimeId still exists in Hand and Presentation needs to know whether any destination outcome can still arrive.

### Formal Hand slot stability while owner != Hand

If a RuntimeId still exists in the displayed historical Hand but its Presentation owner is not `Hand`:

- retain exactly one formal Hand child/slot at the frozen matching index;
- keep the formal Widget `ESlateVisibility::Hidden`, not `Collapsed`;
- keep input disabled;
- do not omit/remove that structural child while the historical Hand entry exists.

This preserves current child-count/index validation contracts.

The formal Hand Widget is a historical structural slot. It is not necessarily the current visible card owner.

### Legacy confirmed-position handoff is non-authoritative

Do **not** treat confirmed source center/transform restoration as a second continuity contract.

`ConfirmedCardCenters`, position-only handoff leases or equivalent data may temporarily remain during staged migration, but only as compatibility implementation detail while old SingleRecord paths are being replaced.

A Hand rebuild/reconcile MUST NOT be designed to restore SelectionArea-owned cards to confirmed transforms. SelectionArea-owned visual continuity survives outside formal Hand ownership.

Once the generic transition engine can consume SelectionArea sources, compatibility position-handoff paths should be removed.

## SelectionArea Host

The SelectionArea must have a stable production host separate from normal Hand and PlayArea surfaces.

Initial contract:

```text
SelectionAreaHost
- runtime-created persistent HUD Overlay/Canvas-compatible layer
- owned by Native Selection HUD lifetime
- NOT HB_Hand
- NOT OV_PlayArea
- survives Hand reconcile/rebuild
- survives closing the interactive selection overlay after Confirm
- stable HUD coordinate space
```

The Host may remain allocated while empty; do not destroy it merely because Confirm closed the interactive backdrop.

Pending selected card visuals may be hit-testable for deselect/reselect. Confirmed selected visuals remain visible but input-disabled/HitTestInvisible while waiting for committed/direct-state outcome.

The generic transition presenter may transactionally reparent/take the exact visual from this Host after playback acceptance.

Do not silently reuse `OV_PlayArea` as SelectionArea.

## Ownership State Channel and Snapshot Ordering

Card Presentation ownership is transient Presentation state with its **own** mutation/dirty channel. It MUST NOT rely on `FPresentationStateSnapshot` publication as its only notification source.

Behavior must provide an event/change descriptor equivalent to:

```text
OnCardPresentationOwnershipChanged
or
TransientPresentationDirty(CardOwnership, RuntimeIds...)
```

The exact API may differ, but these changes must update affected surfaces immediately even when historical state did not change:

```text
Hand → SelectionArea(Pending)       // select click
SelectionArea → Hand                // deselect
Pending → Confirmed                 // Confirm
SelectionArea → Transition          // playback accepted
Transition → ConsumedPendingReducer // group child completion
```

Ownership state may live inside `UBattleHUDViewModel` or in a dedicated transient Presentation state object owned beside it. Either way:

- it is not copied from `FPresentationStateSnapshot`;
- normal `ApplyPresentationSnapshot()` MUST NOT overwrite/reset ownership merely because historical fields or revision changed;
- snapshot application first copies historical display state, then reconciles ownership against the new displayed state and completion watermarks;
- ownership reconciliation produces its own ownership dirty notification;
- if historical and ownership changes happen in one operation, publish them coherently/batched so Hand and SelectionArea never observe a transient duplicate-visible or ghost state.

Formal Hand presentation must listen to ownership dirty changes because visibility/input can change without Hand array changes. SelectionArea presentation must listen to the same lifecycle channel because its visuals can appear/disappear without any historical snapshot publication.

## Ownership Reconciliation

Presentation ownership MUST be recoverable even when animation never starts or fails.

After displayed frozen state changes, reconcile exact ownership entries:

```text
RuntimeId absent from displayed Hand
→ reducer/direct baseline/final snapshot consumed it
→ clear stale SelectionArea/Transition/Consumed ownership
```

For a RuntimeId that still exists in Hand, recovery to `Hand` is permitted only when all are true:

```text
confirmed lifecycle completion watermark is reached
AND no destination ownership/transition remains pending
AND entry still belongs to the exact BattleId + SelectionGeneration + boundary
```

Then:

```text
degradation recovery
→ owner = Hand
```

Request/resolver disappearance alone MUST NOT satisfy the watermark.

This reconciliation is required for Widget decline, unsupported destination, malformed/zero-member continuation, no-history/direct-baseline mode, timeout, skip/collapse, Widget replacement and battle replacement.

Never leave a ghost SelectionArea card solely because no visible transition callback occurred.

## Selection UI Responsibility

`UBattleHUDSelectionWidget` and related Selection UI code should own player interaction and SelectionArea presentation only:

```text
candidate affordance
select/deselect
Confirm/Cancel
SelectionArea layout/host
Pending/Confirmed interaction phase
```

Destination-specific animation does not belong to the Selection interaction subclass.

Do not add new Selection-subclass branches such as:

```text
if destination == DrawPile ...
if destination == Exhaust ...
if CardId == Warcry ...
```

Generic card-transition presentation owns current visual owner → committed destination.

## Generic Card Transition Responsibility

SingleRecord and Group card transitions must converge on one per-child transition engine.

A source resolver may consume an exact RuntimeId from `Hand`, `SelectionArea` or another explicitly supported Presentation owner. Destination animation is determined from immutable committed zone facts.

Native transition state must support N child instances for Group playback; do not preserve a global one-moving-card data model as the concurrency architecture.

## Controller / Widget Boundary

Controller owns committed chronology and semantic Group validation. UI owns visual state.

Controller semantic Group preflight may inspect only immutable Envelope data:

```text
group metadata
member count/shape
PresentationSequence
RuntimeId uniqueness
reducer dry-run
future-member interference
```

Controller MUST NOT query concrete SelectionArea Widget ownership or geometry.

Concrete HUD visual preflight occurs inside the hardened `PlayPresentationGroup(...)` path and validates exact visual owner, geometry, destination anchors and all child construction transactionally.

On visual rejection, return false and let Controller degrade without partially transferring ownership.

On group visual completion, Presentation/UI may move exact future members to `ConsumedPendingReducer` before forwarding Base Widget completion. Controller tracks visually-presented Record indices but does not directly mutate card Presentation ownership.

## Committed Presentation

The committed-history flow remains:

```text
Record payload
→ transient Presentation
→ exact-token completion callback
→ chronological reducer advances WorkingPresentationSnapshot
→ displayed snapshot reconciliation
→ Envelope FinalSnapshot reconciliation
```

PresentationId is visual mapping identity, not Gameplay identity. Historical Status identity uses `TargetPresentationId + StatusId + RuntimeSequence`; never match only by StatusId or array index.

Resolved combatant PresentationIds are non-empty, battle-scoped unique and immutable after the first exact frozen baseline.

`PresentationUnavailable` still initializes enough ViewModel state for the normal HUD/error surface, disables input and exposes a clear development-facing error.

## Input and Interaction

Phase 6UI-A uses explicit card selection followed by legal-target selection. Enemy-target and Self-target cards use Gameplay-provided public LegalTargets. Widget mapping uses PresentationId; Requests submit current runtime Gameplay identity and revalidate.

Presentation may lock the View while Gameplay is request-eligible. Unlock only after Controller catches up to the newest matching revision and authoritative Gameplay remains request-eligible.

A single physical input event must not cross Presentation/Selection/Confirm state boundaries.

## Preview Phase Boundary

UI-A2E and UI-A3 are sealed. The Preview contracts below remain applicable; `docs/Phase6UIA3CardFacePreviewAmendment.md` controls visible A3-5 behavior where it differs from the original implementation document. Current task scope comes from the user request and applicable current design.

Use the name **Target-Specific Current-State Preview**. Preview construction belongs to a Gameplay/read Query boundary. ViewModel/UMG own selection, preview-target nomination, hover/focus, clearing and display only; they must not iterate CardEffects or reimplement Damage/Block/Energy legality rules.

The first Preview model uses a flat Blueprint-friendly Operations array. It reports supported values for current `(BattleId, StateRevision, Card, Target)` and does not promise the final whole-resolution outcome. Damage preview is resolved incoming damage per hit before Block absorption, not predicted HP loss.

Visible A3-5 Preview belongs to the currently selected Native Hand card. Gameplay-resolved supported operation values may temporarily replace matching semantic values in that card face. Do not render a separate Damage/Block/Energy Preview label.

`OV_PlayArea` is committed A2-only. A3 Preview must never add a child to it or reuse A2 damage-number presentation as a pre-commit surface.

Energy/cost remain valid ImmediatePreview DTO fields for Gameplay-owned legality but are not a standalone visible A3-5 loss preview.

Inspection and Preview are separate lifecycles. Do not reuse combatant/status inspection events as PreviewTarget ownership merely because hover may drive both.

On BattleId/StateRevision change, clear ordinary card selection, legal targets, PreviewTarget and Preview according to their own lifecycle. Card-selection Presentation ownership follows its explicit BattleId/SelectionGeneration/completion-watermark reconciliation contract rather than being implicitly destroyed by every historical field update.

On accepted authoritative ordinary-card submission, restore/clear pre-commit card-face Preview before committed playback takes visual ownership.