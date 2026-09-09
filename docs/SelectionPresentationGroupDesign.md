# Selection Presentation Group + Parallel Animation Design

Date: **2026-09-09**

Status:

```text
DRAFT / CODE-ALIGNED REVIEW INCORPORATED /
COMPLETION-WATERMARK + OWNERSHIP-DIRTY CONTRACT DEFINED /
G8 EARLY-INPUT / PRESENTATION-PIPELINING TARGET DEFINED /
G0 IMPLEMENTED WITH AUTOMATED EVIDENCE / G0 MANUAL PIE PENDING /
NO PRODUCTION GROUP CODE IMPLEMENTED / G1-G8 NOT VALIDATED / NOT SEALED
```

Related contracts:

- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`
- `docs/SelectionPresentationGroupImplementationPlan.md`

## Implementation baseline and review findings — 2026-09-09

Reviewed against HEAD `ce36e56bd540a3e6c9cf977aa353e71f2054b081`.

| Area | Code evidence | Consequence for the plan |
|---|---|---|
| G0 A/B | `BattleHUDReconciledWidget`, `BattleHUDViewModel::ApplyPresentationSnapshot` | Incremental dirty and Battle-scoped reuse already exist; preserve completed-draw hit-test adoption. |
| G0 C | `BattleHUDViewModelPresentationOwnership.cpp` | Exact completion set and standalone lifecycle APIs exist; production Selection and Controller do not yet wire them. |
| Current Selection | `BattleHUDSelectionWidget::HandleSelectionAwareConfirmClicked` | Uses fallible bool submission and compatibility centers; rejected Confirm must remain interactive after migration. |
| Interactive boundary | `AdvancePresentationAtInteractiveSelectionBoundary`, `DeferredSelectionAction`, queue writer rebinding | Pre-choice Envelope seals before the continuation writer opens; never infer the outcome from the displayed pre-choice Resolution. |
| Controller recovery | `CompleteActiveEnvelope`, `CollapseToEnvelope`, `ClearPlaybackState` | Completion is not wired to ownership; current collapse clears playback backlog, so it cannot serve the proposed active-envelope recovery unchanged. |
| Group/transition | Record types have no Group protocol; Native card playback has one active instance | G1-G6 are planned work, not functionality established by G0 tests. |

Priority findings corrected below: contradictory completion predicates, missing
Confirm rejection/outcome correlation, insufficient group membership proof,
missing visual transaction details, and G8 lifetime conflict. Existing evidence
is in `docs/SelectionPresentationG0Execution.md` and `docs/Validation.md`; this
documentation review does not claim additional runtime validation.

## 1. Goal

A confirmed multi-card Selection should behave as one visual cohort when safe:

```text
select A/B/C
→ A/B/C visibly live in SelectionArea
→ Confirm
→ A/B/C destination transitions begin together
→ total visible wait ≈ one transition duration
```

while preserving:

```text
Gameplay mutation order
BattleEvent/trigger order
PresentationSequence order
chronological reducer order
```

If parallel co-presentation is unsafe or unavailable, sequential playback must remain visually correct and must never flash later selected cards back to the normal Hand row.

## 2. Primary architecture decision

The selected-card continuity model is not position restoration. It is explicit Presentation ownership:

```text
Hand
→ SelectionArea
→ Transition
→ ConsumedPendingReducer
→ Done
```

No Gameplay `SelectionZone` is introduced.

A card may be:

```text
Gameplay Zone       = Hand
Presentation Owner  = SelectionArea
```

The historical Hand and visible Presentation owner are intentionally separate.

## 3. Non-negotiable invariants

### 3.1 Gameplay is never reordered for visuals

SelectionResult canonical order, direct continuation order, Gameplay mutations, events/triggers and `PresentationSequence` assignment remain unchanged.

### 3.2 Reducer chronology is always committed order

Even when A/B/C animate together:

```text
reduce A
→ chronological interleaved records
→ reduce B
→ ...
→ reduce C
```

No future member is reduced early.

### 3.3 Visible order exception is explicit

Normal visible playback follows `PresentationSequence`.

Only a complete explicit Controller-validated Presentation Group may co-present its own sealed-envelope members. It may not consume/skip interleaved records or infer membership from CardId, Effect type, destination, adjacency, timing or click order.

### 3.4 One RuntimeId has one visible owner

At most one visible Presentation surface owns a RuntimeId at a time.

### 3.5 Ownership is independently reconcilable

This is a top-level invariant:

> Presentation ownership must be reconciliable independently of successful animation playback.

Animation may transfer visible ownership, but reducer/direct-baseline/FinalSnapshot reconciliation must always be able to terminate or restore ownership.

### 3.6 Request completion is not lifecycle completion

The pending Selection request/resolver clearing after Confirm MUST NOT be treated as the end of the Presentation ownership lifecycle.

The lifecycle ends only at an explicit runtime completion watermark defined in Section 5/10.

### 3.7 Ownership changes have their own notification channel

Historical snapshot dirty state and transient Presentation ownership dirty state are distinct.

A select/deselect/Confirm/transition ownership change must be observable even if no `FPresentationStateSnapshot` field changed.

### 3.8 Controller does not own UI visual state

Controller operates on immutable committed semantics. It does not query SelectionArea Widgets, geometry or transient visual ownership and does not directly mutate card Presentation owners.

## 4. Responsibility split

```text
Gameplay
    authoritative Hand/DrawPile/Discard/Exhaust membership
    mutation/event/trigger order

FPresentationRecord / writer
    immutable committed facts
    optional explicit group correlation

BattlePresentationController
    playback chronology
    group semantic discovery/validation
    reducer dry-run
    future-member interference
    VisuallyPresented record bookkeeping
    recovery scope / active-envelope reconciliation
    recorded Resolution completion watermark publication/input

Dedicated transient card Presentation ownership state
    exact RuntimeId ownership lifecycle
    selection generation/boundary identity
    completion watermark
    ownership reconciliation against displayed state
    independent ownership dirty/event publication

UBattleHUDViewModel
    frozen displayed HUD state
    interaction/read-facing state
    may own/embed the transient ownership state implementation,
    but normal FPresentationStateSnapshot copy does not overwrite it

SelectionAreaPresenter
    persistent SelectionAreaHost
    selected visual creation/layout
    Pending/Confirmed interaction phase

Formal Hand presenter
    historical structural slots
    RuntimeId-keyed Hand reconcile
    owner-driven visibility/input

Generic CardTransitionPresenter
    transactional visual preflight
    current visual owner → committed destination
    one child for SingleRecord / N children for Group

UBattleHUDWidgetBase
    exact Record-or-Group playback-unit token ownership
    deferred completion / stale callback rejection / cancel boundary
```

Core rule:

```text
Controller knows Records and completion of recorded Presentation work.
Ownership state knows transient card visual lifecycle.
Presenters know visuals.
Gameplay knows real zones.
```

The ownership state does not have to be a separate UObject, but it MUST be logically independent from frozen snapshot copy/reset semantics and MUST have an independent change channel.

## 5. Ownership lifecycle identity and completion watermark

RuntimeId alone is insufficient to scope stale Selection lifecycle events.

Each ownership entry needs behavior equivalent to:

```cpp
struct FCardPresentationOwnershipEntry
{
    int64 BattleId;
    int64 SelectionGeneration;
    int64 SelectionBoundaryRevision;
    int32 RuntimeId;
    ECardPresentationOwner Owner;
    ESelectionVisualPhase Phase;
    FSelectionPresentationCompletionWatermark CompletionWatermark;
};
```

Exact names/types may differ.

Required phases:

```text
SelectionArea + Pending
→ visible
→ selectable/deselectable

SelectionArea + Confirmed
→ visible
→ input disabled
→ waiting for committed/direct-state outcome
```

A stale callback from another battle, boundary or generation cannot mutate a current entry by RuntimeId alone.

Group identity may be associated after correlation is known but is not required for pending SelectionArea ownership.

### 5.1 Completion watermark modes

The completion watermark is a runtime-comparable proof that the post-confirm continuation can no longer produce later Presentation ownership for that Selection lifecycle.

Behavior must be equivalent to:

```cpp
enum class ESelectionPresentationCompletionMode : uint8
{
    Unresolved,
    RecordedResolution,
    DirectStateRevision
};

struct FSelectionPresentationCompletionWatermark
{
    ESelectionPresentationCompletionMode Mode;
    int64 BattleId;
    int64 SelectionGeneration;
    int64 BoundaryRevision;
    int64 ResolutionId;
    int64 StateRevision;
};
```

The exact type may differ.

### 5.2 Recorded Presentation watermark

For normal recorded Presentation, the watermark is the owning/continuation Resolution that contains the post-confirm continuation work.

The watermark is reached only when that exact Resolution is formally Presentation-complete, including one of these equivalent outcomes:

```text
normal envelope completion
active-envelope FinalSnapshot reconciliation
formal collapse/recovery that marks that Resolution complete
```

Merely observing:

```text
pending request == false
resolver == idle
selection overlay closed
```

is never sufficient.

### 5.3 Direct/no-history watermark

When committed Presentation recording is disabled, there may be no Presentation Envelope/Record to wait for.

The lifecycle instead uses the authoritative post-confirm direct/frozen baseline edge:

```text
watermark = post-confirm StateRevision/baseline identity
```

It is reached only when that authoritative post-confirm state is the displayed state.

### 5.4 Watermark may be unresolved briefly

Immediately at Confirm, the exact recorded ResolutionId or direct post-confirm StateRevision may not yet be available at the UI boundary.

`CompletionMode = Unresolved` is legal temporarily.

Unresolved means:

```text
lifecycle definitely NOT complete
```

It must later be armed with the exact recorded/direct watermark through the formal Presentation/read boundary. It MUST NOT cause fallback restoration.

### 5.5 Outcome correlation is required independently of Group membership

Before G5 activation, provide an immutable outcome correlation across the formal
Selection/read/Presentation boundary. It must associate the exact submitted
request boundary with its accepted continuation ResolutionId, or its strictly
newer direct baseline revision. UI maps that identity to its local
SelectionGeneration; Gameplay does not depend on Widget ownership or generation.
Exact API/DTO layout is an implementation choice in G1, not an existing API.

The correlation must remain available after resolver clearing and for zero
eligible records, one-member SingleRecord, recording disabled, and declined Group
playback. GroupId alone and searching for the first destination Record are
insufficient. Do not guess `ResolutionId + 1`, take the newest Resolution, or use
SelectionSource/CardId/candidate-array equality as a request identity.

The current interactive boundary seals the pre-choice Envelope and rebinds queued
Actions to a continuation writer. Use that verified continuation scope. A later
selection boundary may seal another segment: prove that the correlated segment
contains all direct destination work for this choice before using its completion
as final. If a future continuation spans segments, define an explicit terminal
outcome edge; neither the first segment nor another choice's completion is proof.
Keep correlation local to the bounded selection/continuation lifecycle; no global
registry or Gameplay dependency on Presentation availability is needed.

Recorded completion uses the exact `(BattleId, ResolutionId)` completion set, as
G0 currently does. A numerically greater completed ID is not evidence for an
unobserved ID. Direct completion requires `target revision > boundary revision`
and displayed same-Battle revision at least that target. Armed identity is
immutable except for idempotent re-arming. Global collapse explicitly accounts
for the discarded scopes it terminates; it does not fabricate completion for
every smaller numeric ID.

## 6. SelectionArea Host

SelectionArea must be a persistent production surface, not a render translation inside `HB_Hand`.

Initial Host contract:

```text
SelectionAreaHost
- runtime-created Overlay/Canvas-compatible layer
- owned by Native Selection HUD lifetime
- NOT HB_Hand
- NOT OV_PlayArea
- survives Hand reconcile/rebuild
- survives SetSelectionLayoutActive(false) / overlay close after Confirm
- stable HUD coordinate space
```

The Host may remain allocated while empty.

Pending visuals may be hit-testable for deselection. Confirmed visuals are visible but input-disabled/HitTestInvisible.

The generic transition presenter may transactionally take/reparent the exact visual from this Host after playback acceptance.

No production `.uasset` change is required merely to create this runtime Host.

## 7. Formal Hand structural lifetime

While current historical contracts depend on Hand child count/index:

```text
one WorkingSnapshot.HandCards entry
↔ one formal HB_Hand child at same historical index
```

If owner != Hand:

```text
formal child = Hidden
input = disabled
slot/index retained
```

Do not omit/remove/Collapse it while the historical Hand entry remains.

The formal Hand Widget is a structural historical placeholder, not necessarily the visible owner.

## 8. RuntimeId-keyed Hand reconciliation

The redesign should remove unconditional full Hand destruction as a correctness mechanism.

Target reconciliation:

```text
old RuntimeIds vs new RuntimeIds
→ reuse survivors
→ remove missing
→ create added
→ reorder slots if needed
→ apply visibility from Presentation ownership
```

This preserves stable Widget identity, geometry and local state for surviving cards.
Widget identity does not guarantee unchanged cached geometry after slot
reattachment. Revalidate geometry when preparing a transition. Current DrawToHand
temporarily appends one presentation-only child; the exact historical count/index
contract applies at stable reconciled/preflight boundaries. Its formal adoption
must restore child hit testing and request binding, then apply explicit ownership.

Historical indexes remain valid for record/snapshot validation but are not the primary live visual lookup.

```text
Historical index → validates frozen order
RuntimeId        → identifies visual/ownership
```

## 9. Selection lifecycle

### 9.1 Select

```text
owner A: Hand → SelectionArea(Pending)
```

Gameplay stays Hand. Formal Hand A remains Hidden structural slot. SelectionAreaHost owns the visible A.

This mutation emits ownership dirty state immediately; it does not wait for a new historical snapshot.

### 9.2 Deselect

```text
SelectionArea(Pending) → Hand
```

No Gameplay mutation. Ownership dirty is emitted immediately.

### 9.3 Confirm

```text
SelectionArea(Pending)
→ SelectionArea(Confirmed)
→ submit authoritative SelectionResult
```

Closing the interactive overlay MUST NOT destroy confirmed selected visuals.

Confirm emits ownership dirty for the phase change. It associates the exact lifecycle with a completion watermark once the recorded/direct outcome identity becomes knowable. Until then the watermark remains Unresolved and the lifecycle is not complete.

The diagram is the successful path. Actual submission is fallible and may run
continuation work synchronously. Prepare the exact lifecycle without destroying
Pending visuals; submit through the formal Request boundary, then commit
Confirmed only on acceptance. Any provisional phase/input change must be
reversible and notifications buffered so re-entrant state/outcome callbacks
cannot observe half a transaction. Do not invoke G0's irreversible Confirm API
before a request whose rejection has no rollback path.

On rejection with the same exact pending request, preserve choices and restore
Pending input. On request replacement or internal failure, reconcile the exact
old lifecycle against the new authoritative boundary; do not restore stale
choices into the new request. Accepted submission followed by missing outcome
metadata must have an explicit unavailable/recovery path, never an indefinitely
Unresolved ghost. G5 tests must cover all these dispositions.

### 9.4 Single/Group transition accepted

```text
SelectionArea(Confirmed) → Transition
```

Only actual accepted visual ownership transfer changes the owner. This emits ownership dirty immediately.

### 9.5 Group child animation finishes before reducer cursor

```text
Transition → ConsumedPendingReducer
```

No visible card remains. Formal historical Hand slot remains Hidden while the working snapshot still contains it. This emits ownership dirty immediately even though historical Hand is unchanged.

### 9.6 Chronological state consumes RuntimeId

When the displayed historical state no longer contains that RuntimeId in Hand:

```text
ownership reconcile → Done
```

Controller does not directly clear the owner.

## 10. Ownership reconciliation

Reconciliation runs whenever displayed authoritative/frozen state changes or a terminal Presentation lifecycle boundary/watermark advances.

Snapshot publication order is:

```text
copy/accept historical displayed state
→ calculate historical dirty
→ reconcile transient card ownership against new state + watermark
→ calculate ownership dirty
→ publish coherent/batched notifications
```

No intermediate publish may expose both the formal Hand card and SelectionArea card as visible, or neither when recovery should restore Hand.

This is a final observable/rendered-state guarantee, not proof supplied by two
multicast events. G0 reconciles data before publishing historical then ownership
dirty; separate future surface listeners could still observe partially updated
Widgets. G5/G6 must stage all affected visual changes under one transaction,
release the old visible owner before revealing its replacement, and defer
external callbacks until commit. Never depend on listener registration order.

### 10.1 Consumed in displayed state

```text
ownership RuntimeId absent from displayed Hand
→ clear SelectionArea/Transition/ConsumedPendingReducer entry
→ release stale visual object if any
```

This rule does not need to wait for lifecycle completion watermark because the displayed state already proves the card left Hand.

This covers normal reducer completion, Widget decline followed by reducer advance, direct/no-history baseline, FinalSnapshot reconciliation and skip/catch-up.

### 10.2 RuntimeId remains in Hand: exact completion test

The old natural-language condition “confirmed lifecycle definitively ended” is replaced by this runtime predicate:

```text
CanRestoreConfirmedOwnerToHand(entry, displayedState) =
    entry.Phase == Confirmed
    AND entry belongs to current BattleId + SelectionGeneration + boundary
    AND entry.CompletionWatermark is resolved
    AND entry.CompletionWatermark is reached
    AND RuntimeId still exists in displayed Hand
    AND exact visual work has been finished/cancelled before completion publication
```

Only then:

```text
degradation recovery
→ owner = Hand
→ remove SelectionArea visual
→ formal Hand child becomes visible/input follows normal rules
```

Recorded mode `watermark reached` means:

```text
CompletedResolutionIds.Contains(exact owning ResolutionId)
for the same BattleId
```

where completion includes normal completion or formal FinalSnapshot/collapse recovery.

For G0-G7, completion is a terminal visual boundary. A leftover Transition or
ConsumedPendingReducer enum is stale bookkeeping, not grounds to withhold
recovery forever. Match current G0 behavior: recover any Confirmed non-Hand owner
still in Hand once its exact watermark is reached. Production completion wiring
must cancel/release exact visual work first, then reconcile data and surfaces
coherently. Pending entries instead expire on their stale selection boundary;
they do not wait for a Confirmed watermark.

Direct mode `watermark reached` means:

```text
displayed BattleId/StateRevision has reached the exact authoritative
post-confirm direct baseline watermark for this lifecycle
```

The following are explicitly insufficient:

```text
request cleared
resolver no longer pending
Confirm button hidden
selection overlay closed
input selection state cleared
group not found
```

This is the zero-member degradation path:

```text
Confirm B
→ owner B = SelectionArea(Confirmed)
→ B produces zero eligible destination records
→ owning recorded Resolution still runs/completes
→ FinalSnapshot/completion watermark reached
→ B still exists in displayed Hand
→ exact visual work finished/cancelled at completion
→ B owner = Hand
```

### 10.3 Battle/widget/global boundaries

Battle replacement, stale generation, Widget destruction/replacement, PresentationUnavailable, direct-baseline mode and global skip/collapse must terminate or restore ownership through exact reconciliation and/or explicit watermark advancement appropriate to that recovery path.

### 10.4 Ownership dirty/event channel

Transient ownership mutation has an independent notification channel conceptually equivalent to:

```text
OnCardPresentationOwnershipChanged(ChangedRuntimeIds, ChangeKind)
```

or a transient Presentation dirty descriptor.

It is separate from historical snapshot dirty state.

At minimum it must notify:

```text
Formal Hand presenter
    owner-driven visibility/input changes

SelectionAreaPresenter
    visual create/remove/phase/interactivity changes

CardTransitionPresenter
    ownership transfer/cleanup where relevant
```

Normal `ApplyPresentationSnapshot()` MUST NOT reset ownership because `StateRevision` changed. It may reconcile ownership **after** copying the historical state.

The ownership state may be embedded in ViewModel implementation or stored in a dedicated Presentation state object, but it must have separate lifecycle storage and separate notification semantics from frozen `FPresentationStateSnapshot` fields.

## 11. Legacy handoff compatibility

The previous model:

```text
ConfirmedCardCenters
position-only handoff lease
RefreshHand restores confirmed transform
```

is superseded as an architectural contract.

It may remain temporarily only while old production SingleRecord animation paths still require it during staged migration.

It is not a second ownership truth and must be deleted after the SelectionArea source migration is validated.

## 12. Group metadata

Grouping is explicit committed Presentation metadata.

Conceptually:

```cpp
enum class EPresentationGroupKind : uint8
{
    None,
    SelectionDestination
};

struct FPresentationGroupTag
{
    EPresentationGroupKind Kind;
    int64 GroupId;
    int32 ExpectedMemberCount;
};
```

Identity:

```text
(BattleId, ResolutionId, Kind, GroupId)
```

`PresentationSequence` remains canonical member order.

Initial eligible member:

```text
CardZoneChanged
FromZone = Hand
Kind = SelectionDestination
```

The tag above is only a record reference, not the full validity proof. The sealed
Envelope must also carry a group declaration with the canonical selected
identities (or equivalent recorder-validated evidence), not merely their count.
G2 compares the member identity set with that declaration and rejects omissions,
substitutions and duplicates. A declaration can have zero emitted members so
missing outcomes are diagnosable; lifecycle completion still uses section 5.5,
not discovery of a tagged leader. Do not copy UObject pointers into this metadata.

## 13. Writer-scoped GroupId allocation

Only the frozen writer capability may allocate:

```cpp
bool FPresentationRecordWriter::TryAllocatePresentationGroupId(int64& OutGroupId) const;
```

The recorder validates writer BattleId/ResolutionId against the active resolution before allocating from an active-resolution-scoped counter.

A stale writer cannot allocate from a newer resolution.

Writer allocation does not imply automatic group propagation.

## 14. Direct continuation correlation

`USelectionRequestAction` remains object-type neutral.

After resolution, canonical selected candidate RuntimeSequences are mapped. For current-Hand candidates, RuntimeSequence corresponds to exact CardRuntimeId.

Direct continuation Actions receive optional Action-local group context.

Trigger reactions inherit the ordinary Presentation writer but MUST NOT inherit Selection group context.

Allocate correlation using the post-boundary writer. Context propagation to
direct children is explicit; writer propagation/rebinding alone must not spread
group context to reactions, retries or a later independent Selection. A missing
writer or optional group allocation/validation failure disables grouping while
preserving all ordinary records and Gameplay behavior; it cannot fault Gameplay
or discard valid serial history.

A concrete card-zone Action stamps a group only when its committed exact RuntimeId matches the direct selected set.

## 15. Exact-one-member eligibility

Initial grouped semantics require:

```text
one selected RuntimeSequence
→ exactly one eligible direct group CardZoneChanged record
```

If a selected member produces zero or more than one eligible matching record, that group is invalid for co-presentation and degrades.

`ExpectedMemberCount <= 1` remains SingleRecord Controller playback initially.

## 16. Controller semantic Group preflight

Controller uses only sealed immutable Envelope data.

At the first tagged member, semantic co-presentation eligibility requires:

1. `ExpectedMemberCount > 1`;
2. exact discovered member count equals expected count;
3. identical group identity/count metadata;
4. supported committed member shape;
5. unique RuntimeIds;
6. unique valid `PresentationSequence` values;
7. exact-one-member eligibility;
8. no terminal misuse;
9. chronological reducer dry-run succeeds through final member;
10. future-member interference preflight succeeds.

Controller MUST NOT check SelectionArea owner/Widget/geometry/destination anchors.

Semantic failure marks the group sequentially disabled without Gameplay fault.

## 17. Future-member interference

Reducer validity alone cannot authorize early visible consumption.

For every future group member, inspect every interleaved record outside this group before that member's reducer position, including records tagged for another group. Foreign tags do not exempt an exact-card conflict. Unknown/new record shapes conservatively reject lookahead until classified.

At minimum:

```text
CardZoneChanged touching future RuntimeId → interference
CardPlayed touching future RuntimeId      → interference
```

Example:

```text
A Hand→Exhaust [G]
B Hand→Discard [ungrouped]
B Discard→Hand [ungrouped]
B Hand→Exhaust [G]
```

Dry-run may be legal but G is visually unsafe and must degrade to sequential.

Unrelated Damage/Status/Energy/other-card records do not disable the group merely by interleaving.

## 18. Widget visual transactional preflight

After Controller semantic validation, the hardened Base Widget offers the Group to concrete Presentation.

Concrete visual preflight validates:

- current exact owner exists for every member where required;
- current visible card object is valid;
- source geometry is valid;
- destination anchor/style is supported;
- every child transition can be prepared.

Preparation is read-only with respect to durable ownership.

If child N fails:

```text
rollback all prepared children
→ zero ownership transferred
→ zero member becomes VisuallyPresented
→ return false
→ Controller sequential fallback
```

Only after all children are prepared and the playback unit is accepted:

```text
SelectionArea → Transition for all members transactionally
→ ownership dirty emitted for all transferred RuntimeIds
→ all children begin in same Native tick
```

## 19. Base Widget playback-unit hardening

Controller-facing playback becomes:

```text
SingleRecord OR Group
```

Both share one exact tracked playback owner in `UBattleHUDWidgetBase`.

Group inherits existing hardening:

- token established before concrete Begin;
- synchronous completion deferred before Controller sequencing resumes;
- stale/duplicate/old-battle callbacks rejected;
- exact timeout/cancel;
- Widget replacement cannot affect a newer owner;
- concrete HUD never directly advances Controller state.

Token identity distinguishes unit kind and GroupId where applicable.

## 20. Generic N-child transition engine

SingleRecord and Group use one generic per-child engine.

Conceptually:

```cpp
struct FCardTransitionInstance
{
    int32 RuntimeId;
    ECardZone FromZone;
    ECardZone ToZone;
    TObjectPtr<UBattleCardWidget> MovingVisual;
    float ElapsedSeconds;
    ...
};
```

Source resolution is owner-aware:

```text
owner == Hand
→ exact Hand visual

owner == SelectionArea
→ exact SelectionArea visual

owner == ConsumedPendingReducer
→ do not replay visible transition
```

Destination style is committed-zone-driven only.

The initial G4 migration includes existing Hand→DrawPile/Discard/Exhaust paths;
Discard is already production behavior and cannot be silently deferred. Preserve
CardPlayed, DrawPile→Hand and PlayArea cleanup through explicit adapters until
their replacement has equivalent coverage; remove only state actually replaced.

Formal structural Widgets remain in Hand while their historical entry exists.
Use a frozen-data SelectionArea visual alongside the Hidden formal slot; do not
reparent the sole formal child and thereby break count/index or G0 identity.
Hand sources may likewise use an in-place or separate moving visual. Once a
SelectionArea visual exists, its transfer to Transition preserves that exact
visible object. This permits a Hidden structural Widget plus one visible owner,
not two visible cards. All prepared/moving visuals need GC-tracked references
through commit, rollback and callback completion. Reparented geometry must be
converted in the common HUD coordinate space and Host availability/layer order
validated before production activation.

SingleRecord uses one instance. Group uses N instances.

## 21. Sequential fallback

If semantic or visual group preparation fails before accepted group ownership:

```text
A/B/C remain SelectionArea(Confirmed)
```

Then:

```text
A SingleRecord accepts
→ A SelectionArea→Transition
→ A completes/reduces/reconciles

B/C remain visibly SelectionArea-owned
→ Hand reconcile cannot steal them back

B SingleRecord
→ ...
```

No confirmed-position reconstruction is required.

If a SingleRecord Widget declines, Controller may reduce normally; displayed-state ownership reconciliation then clears/restores that exact owner as appropriate. Restoration to Hand, if the card remains in Hand, still requires the exact completion watermark from Section 10.2.

## 22. Group normal completion

For accepted A/B/C:

```text
A/B/C SelectionArea→Transition together
→ child animations finish
→ future members become ConsumedPendingReducer
→ ownership dirty emitted
→ exact hardened Group completion callback
```

Controller marks the exact member record indices VisuallyPresented and resumes chronological reducer at the leader cursor.

When reducer later reaches an already visually presented member:

```text
skip visible Begin
→ reduce exact record
→ publish displayed snapshot
→ ownership reconciliation removes exact consumed entry
```

Interleaved ungrouped records still play in chronological position.

## 23. Timeout and recovery scope

Normal completion and timeout are different terminal paths.

Group timeout MUST NOT call leader-style ordinary completion.

Required behavior:

```text
exact Group timeout
→ cancel exact Base Widget playback unit
→ clean all child visuals
→ do not mark failed members VisuallyPresented
→ terminate active-envelope transient ownership
→ atomically apply ActiveEnvelope.FinalSnapshot
→ mark owning Resolution completion watermark reached through formal recovery
→ reconcile Presentation ownership from final snapshot
→ publish coherent historical + ownership dirty state
→ mark active envelope complete
→ preserve later queued envelopes
→ continue backlog
```

Controller needs behaviorally distinct recovery scopes equivalent to:

```text
ActivePlaybackUnit
ActiveEnvelope
EntireBacklog
```

Do not reuse a helper that silently clears the whole backlog when only ActiveEnvelope recovery is intended.

## 24. No-history / direct-baseline behavior

Recording disabled does not disable mandatory Gameplay Selection.

SelectionArea may therefore receive Confirmed ownership without later receiving a recorded committed transition.

The lifecycle must arm a direct-state completion watermark once the authoritative post-confirm baseline/revision is known.

Direct/frozen baseline reconciliation must terminate/restore ownership:

```text
post-confirm direct watermark not yet displayed
→ keep SelectionArea(Confirmed)

post-confirm direct watermark displayed
→ card absent from Hand: clear owner
→ card remains in Hand after exact visual cleanup: owner back to Hand
```

No ghost SelectionArea visual may wait for a nonexistent record.

## 25. Warcry behavior

Warcry remains one-member SingleRecord playback initially:

```text
Hand → SelectionArea(Pending)
→ Confirmed
→ generic SingleRecord source resolver sees SelectionArea
→ SelectionArea→Transition→DrawPile
→ displayed state reconciles ownership
→ Warcry later ordinary PlayArea→Exhaust
```

No Warcry-specific animation.

## 26. Multi-exhaust behavior

For Burning-Pact-style A/B/C:

```text
select A/B/C
→ SelectionArea owns A/B/C
→ Confirm
```

Safe Group:

```text
A fade ┐
B fade ├─ same tick
C fade ┘
→ ConsumedPendingReducer for future members
→ chronological reducer continues
```

Unsafe/declined Group:

```text
A/B/C stay in SelectionArea
→ sequential generic transitions
```

No selected card returns visibly to Hand.

## 27. Explicitly rejected designs

Do not use:

- Gameplay SelectionZone;
- selected flag while Hand remains the visible owner;
- confirmed-position restoration as primary continuity;
- request/resolver disappearance as confirmed-lifecycle completion;
- ownership that can change only when a historical snapshot changes;
- independent authoritative `SuppressedRuntimeIds` truth competing with ownership;
- omit/Collapse historical Hand slots while index contracts remain;
- Controller querying concrete SelectionArea visual state;
- Widget scanning future Envelope records;
- Effect/Card-specific transition branches;
- Gameplay batching/reordering for animation;
- one global moving-card state as the parallel engine;
- Group timeout masquerading as normal leader completion.

## 28. Behavior-safe implementation order

Stages are ordered by production behavior dependency, not merely compile dependency:

```text
G0-A incremental ViewModel/HUD dirty propagation
G0-B RuntimeId-keyed Hand reconcile
G0-C ownership infrastructure
     + lifecycle identity
     + completion watermark (recorded Resolution / direct StateRevision)
     + independent ownership dirty/event channel
     + reconciliation
     + persistent SelectionAreaHost
     dormant/compatibility only; production selection source remains old path

G1   Group metadata + writer-scoped correlation
G2   Controller semantic group discovery / reducer dry-run / interference
     visible Group playback still disabled
G3   Base Widget playback-unit hardening + recovery scopes

G4   generic card transition engine
     source resolver supports Hand | SelectionArea
     migrate existing SingleRecord production transitions first

G5   production SelectionArea switch
     selected visible owner leaves Hand
     Confirm keeps durable SelectionArea owner
     old confirmed-center path stops being primary

G6   N-child Group playback + ConsumedPendingReducer + parallel enable

G7   remove compatibility handoff / Selection-specific destination animation
     optional Status keyed reconcile cleanup
     build / automation / PIE / docs / seal work

G8   Presentation pipelining / early input
     separate blocking interaction barriers from non-blocking visual jobs
     allow a newer legal player request while older non-blocking visuals remain alive
     preserve strict Gameplay and reducer chronology
```

G0-C must not turn on production SelectionArea ownership before G4 can consume SelectionArea sources. If necessary, G4+G5 may land as one coherent behavior-safe migration rather than exposing a broken intermediate production state.

G8 is explicitly downstream of the G0-G7 ownership/transition foundation. It MUST NOT be used to justify weakening current interactive Presentation boundaries before the earlier stages are validated.

## 29. Acceptance plan

Focused coverage must include:

### Ownership/reconciliation

- Hand→SelectionArea select without Gameplay mutation and without historical snapshot publication;
- SelectionArea→Hand deselect without historical snapshot publication;
- Pending→Confirmed emits ownership dirty while request clearing does not restore Hand;
- selection generation rejects stale callbacks;
- persistent Host survives Hand reconcile and overlay close;
- formal historical slot remains Hidden/not Collapsed while owner!=Hand;
- recorded lifecycle watermark stays unreached after request clears and reaches only after exact Resolution completion/reconciliation;
- direct lifecycle watermark reaches only when exact post-confirm StateRevision baseline is displayed;
- unresolved watermark never restores owner;
- Widget decline followed by reducer clears ghost ownership;
- no-history/direct baseline clears/restores ownership;
- zero eligible destination record restores only after exact completion watermark;
- battle/widget/global replacement cannot leave stale owner;
- normal snapshot copy does not overwrite transient ownership;
- historical dirty + ownership dirty publish coherently without duplicate-visible/ghost frame.

### Hand foundation

- non-Hand snapshot changes do not recreate Hand;
- surviving RuntimeIds preserve Widget identity across Hand change;
- historical index validation stays correct;
- live visual lookup uses RuntimeId/owner rather than array index;
- owner visibility changes apply from ownership dirty even when `HandCards` is unchanged.

### Group semantics

- complete group validates;
- exact-one-member invariant;
- triggers do not inherit group context;
- unrelated non-contiguous interleaving allowed;
- future exact-card interference disables group;
- Controller never queries concrete visual ownership.

### Visual transaction

- group child N prepare failure rolls back every child and transfers zero owner;
- SingleRecord can source from SelectionArea;
- Group N children begin same Native tick;
- grouped future members become ConsumedPendingReducer;
- sequential fallback leaves remaining cards visibly in SelectionArea.

### Hardening/recovery

- Record and Group share one exact Base playback owner;
- stale callbacks cannot cross unit kinds;
- Group timeout does not leader-complete;
- active-envelope failure preserves later backlog;
- active-envelope recovery advances the recorded completion watermark for the recovered Resolution;
- no one-frame Hand flash during final-snapshot reconciliation.

### Integration

- Warcry SelectionArea→DrawPile then ordinary PlayArea→Exhaust;
- multi-exhaust parallel when safe;
- no CardId/Effect-specific branches;
- Gameplay/reducer chronology remains authored.

G0 evidence is linked in the implementation baseline above. No G1-G8 Build,
Automation or PIE result is claimed by this design document.

## 30. G8 target architecture — Presentation Pipelining / Early Input

G8 is a later responsiveness phase intended to support Slay-the-Spire-style input pacing:

```text
play card A
→ Gameplay A resolves completely and produces the next legal interaction state
→ A still has non-blocking visual work on screen
→ player may already play card B
→ A/B visual work may overlap
```

This does **not** permit concurrent authoritative Gameplay resolutions. Gameplay remains serial:

```text
A Gameplay resolution complete
→ B request accepted
→ B Gameplay resolution
```

Only Presentation lifetimes may overlap across already-committed revisions/resolutions.

### 30.1 Record lifetime and visual lifetime become distinct

G8 explicitly separates:

```text
committed Record/reducer lifetime
from
visual job lifetime
```

A non-blocking visual may continue after its committed record has been reduced and after a newer Gameplay request becomes legal.

Conceptually:

```cpp
struct FPresentationPlaybackJob
{
    FPresentationPlaybackToken Token;
    int64 BattleId;
    int64 SourceResolutionId;
    int64 SourceStateRevision;
    EPresentationInteractionPolicy InteractionPolicy;
    // visual children / ownership references / completion state
};

enum class EPresentationInteractionPolicy : uint8
{
    Blocking,
    NonBlocking
};
```

Exact names/types may differ.

### 30.2 InteractionBarrier

G8 MUST NOT interpret `Gameplay request-eligible` as automatic input unlock.

Some Presentation remains interaction-blocking because the player does not yet have a correct visual decision surface.

Examples that may require a barrier include:

```text
Draw/Hand catch-up before a Selection that depends on newly drawn cards
pending Selection presentation
pending target-choice presentation
an exact card visual still being transferred when that same visual must become the next interaction source
terminal/collapse/recovery boundaries
```

Cosmetic or already-detached work is a candidate for NonBlocking playback, for example:

```text
damage numbers
hit flashes
late PlayArea→destination cleanup
an already detached card flying toward a pile
pure status VFX when it does not define the next decision surface
```

Classification is semantic Presentation policy, not CardId-specific behavior.

### 30.3 InteractionReadyWatermark

G8 introduces a separate readiness watermark. It is **not** the Selection ownership completion watermark from Section 5.

The purpose is to answer:

> Has the exact Gameplay revision reached a state where the player has both authoritative permission and a complete enough visual interaction surface to submit another request?

Conceptually:

```text
InteractionReady(BattleId, StateRevision) =
    authoritative Gameplay is request-eligible for that exact revision
    AND displayed/read-facing interaction state corresponds to that revision
    AND all InteractionBarriers required for that revision are cleared
```

Only then may normal player input unlock.

Older NonBlocking `FPresentationPlaybackJob`s may remain alive after this watermark is reached.

### 30.4 Cross-resolution visual overlap

Once `InteractionReady(R)` is reached:

```text
Resolution A non-blocking visual job ────────────────→

                         player submits B
                         Resolution B commits
                         B visual job ────────────────→
```

The Presenter/visual scheduler therefore eventually needs to track multiple active visual jobs rather than one global active moving-card/record visual.

The existing G4/G6 multi-instance transition engine is a prerequisite and should be reused rather than replaced.

### 30.5 Ownership remains exact during overlap

G8 depends on the ownership model established earlier.

Example:

```text
A owner = Transition        // older visual still flying
B/C/D owner = Hand          // current interactive Hand
```

Hand reconciliation MUST NOT reclaim A merely because a newer Hand snapshot is displayed. Conversely, A's old visual job MUST NOT block B/C/D input unless it owns a declared InteractionBarrier for the current revision.

A stale older job cannot mutate ownership belonging to a newer BattleId/generation/revision.

**Design amendment required before G8 activation:** this example is incompatible
with G0-G7's immediate Hand-absence cleanup and Resolution-completion recovery.
Do not simply reuse those selection entries to keep detached tails alive. Define
a distinct exact visual-job lifetime, with frozen display data and a token that
cannot target the current formal Widget. Releasing a selection entry on reducer
consumption must not destroy an intentionally retained cosmetic job; conversely,
an old job must never reclaim a RuntimeId redrawn into Hand at a newer revision.

Selection completion, reducer completion and visual-job completion need separate
proofs. Old jobs may update only their private cosmetic surfaces, not current
HP/Energy/pile/status/Hand widgets. First permit isolated damage-number/hit-flash
tails; keep card transfers Blocking until same-RuntimeId return, visual ownership,
cleanup and resource limits have a dedicated tested contract. This is a deferred
extension, not permission to weaken the G0-G7 recovery rule now.

### 30.6 Controller/scheduler boundary

G8 should evolve from:

```text
one active playback must finish before controller/UI can become interactive
```

toward:

```text
chronological reducer / semantic controller
+
zero or more active visual jobs
+
explicit interaction barriers
```

Reducer chronology remains exact committed order. A visual job being NonBlocking never authorizes reducer reordering, Gameplay batching, trigger reordering or speculative future state.

### 30.7 Failure, skip and battle replacement

Skip/collapse/replacement must have exact policy for both:

```text
blocking barrier ownership
non-blocking visual jobs
```

At minimum:

- battle replacement cancels every older-battle visual job and barrier;
- stale job callbacks cannot complete a newer barrier/job;
- global Skip may finish/cancel cosmetic jobs according to explicit policy but must leave the displayed authoritative state coherent;
- an individual non-blocking job failure must not relock an already reached InteractionReady watermark unless a genuine current-revision barrier is affected.

### 30.8 G8 acceptance target

G8 is complete only when tests/PIE prove at minimum:

- card A Gameplay resolves before card B request is accepted;
- card B can be played while an explicitly NonBlocking visual from A is still running;
- A/B visual jobs coexist without sharing one global animation state;
- input never unlocks before the exact current revision's InteractionBarriers clear;
- Draw→Selection and target-choice boundaries remain blocking where required;
- old Resolution visuals cannot consume/corrupt current Hand ownership;
- stale visual callbacks cannot affect a newer BattleId/revision;
- reducer/event/trigger order remains identical to the non-pipelined authored order;
- skip, timeout and battle replacement clean overlapping jobs without ghost visuals or permanent input lock.

G8 is a responsiveness optimization over a correct G0-G7 foundation. It is not a prerequisite for Selection Group correctness and must not be implemented by simply setting `bInputLocked = false` while Presentation is active.
