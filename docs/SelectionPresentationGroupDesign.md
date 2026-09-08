# Selection Presentation Group + Parallel Animation Design

Date: **2026-09-09**

Status:

```text
DRAFT / OWNERSHIP-LIFECYCLE REVIEW CLOSED IN DESIGN /
AUTHORITATIVE CONTRACTS ALIGNED / NO PRODUCTION GROUP CODE IMPLEMENTED /
NOT VALIDATED / NOT SEALED
```

Related contracts:

- `docs/CardSelectionPresentationConstraints.md`
- `docs/CardSelectionRefactorConstraints.md`
- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`
- `docs/SelectionPresentationGroupImplementationPlan.md`

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

### 3.6 Controller does not own UI visual state

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

HUD ViewModel or dedicated transient Presentation state
    exact card Presentation ownership lifecycle
    selection generation/boundary identity
    ownership reconciliation against displayed state

SelectionAreaPresenter
    persistent SelectionAreaHost
    selected visual creation/layout
    Pending/Confirmed interaction phase

Formal Hand presenter
    historical structural slots
    RuntimeId-keyed Hand reconcile
    owner-driven visibility

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
Controller knows Records.
Presentation state knows ownership.
Presenters know visuals.
Gameplay knows real zones.
```

## 5. Ownership lifecycle identity

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

### 9.2 Deselect

```text
SelectionArea(Pending) → Hand
```

No Gameplay mutation.

### 9.3 Confirm

```text
SelectionArea(Pending)
→ SelectionArea(Confirmed)
→ submit authoritative SelectionResult
```

Closing the interactive overlay MUST NOT destroy confirmed selected visuals.

### 9.4 Single/Group transition accepted

```text
SelectionArea(Confirmed) → Transition
```

Only actual accepted visual ownership transfer changes the owner.

### 9.5 Group child animation finishes before reducer cursor

```text
Transition → ConsumedPendingReducer
```

No visible card remains. Formal historical Hand slot remains Hidden while the working snapshot still contains it.

### 9.6 Chronological state consumes RuntimeId

When the displayed historical state no longer contains that RuntimeId in Hand:

```text
ownership reconcile → Done
```

Controller does not directly clear the owner.

## 10. Ownership reconciliation

Reconciliation runs whenever displayed authoritative/frozen state changes or a terminal Presentation lifecycle boundary occurs.

### 10.1 Consumed in displayed state

```text
ownership RuntimeId absent from displayed Hand
→ clear SelectionArea/Transition/ConsumedPendingReducer entry
→ release stale visual object if any
```

This covers normal reducer completion, Widget decline followed by reducer advance, direct/no-history baseline, FinalSnapshot reconciliation and skip/catch-up.

### 10.2 Confirmed lifecycle ends but card remains in Hand

If:

```text
RuntimeId still in displayed Hand
AND confirmed lifecycle definitively ended
AND no pending destination ownership exists
```

then:

```text
degradation recovery
→ owner = Hand
→ remove SelectionArea visual
→ formal Hand child becomes visible
```

This prevents ghost cards when a selected object produces zero eligible destination records or another malformed/unsupported path ends without visual playback.

### 10.3 Battle/widget/global boundaries

Battle replacement, stale generation, Widget destruction/replacement, PresentationUnavailable, direct-baseline mode and global skip/collapse must terminate or restore ownership through exact reconciliation.

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

For every future group member, inspect interleaved ungrouped exact-card records before that member's reducer position.

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

If a SingleRecord Widget declines, Controller may reduce normally; displayed-state ownership reconciliation then clears/restores that exact owner as appropriate.

## 22. Group normal completion

For accepted A/B/C:

```text
A/B/C SelectionArea→Transition together
→ child animations finish
→ future members become ConsumedPendingReducer
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
→ reconcile Presentation ownership from final snapshot
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

Direct/frozen baseline reconciliation must terminate/restore ownership:

```text
card absent from resulting Hand → clear owner
card remains in Hand and selection lifecycle ended → owner back to Hand
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
G0-C ownership infrastructure + lifecycle identity + reconciliation + persistent SelectionAreaHost
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
```

G0-C must not turn on production SelectionArea ownership before G4 can consume SelectionArea sources. If necessary, G4+G5 may land as one coherent behavior-safe migration rather than exposing a broken intermediate production state.

## 29. Acceptance plan

Focused coverage must include:

### Ownership/reconciliation

- Hand→SelectionArea select without Gameplay mutation;
- SelectionArea→Hand deselect;
- Pending/Confirmed interaction distinction;
- selection generation rejects stale callbacks;
- persistent Host survives Hand reconcile and overlay close;
- formal historical slot remains Hidden/not Collapsed while owner!=Hand;
- Widget decline followed by reducer clears ghost ownership;
- no-history/direct baseline clears/restores ownership;
- zero eligible destination record cannot strand owner;
- battle/widget/global replacement cannot leave stale owner.

### Hand foundation

- non-Hand snapshot changes do not recreate Hand;
- surviving RuntimeIds preserve Widget identity across Hand change;
- historical index validation stays correct;
- live visual lookup uses RuntimeId/owner rather than array index.

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
- no one-frame Hand flash during final-snapshot reconciliation.

### Integration

- Warcry SelectionArea→DrawPile then ordinary PlayArea→Exhaust;
- multi-exhaust parallel when safe;
- no CardId/Effect-specific branches;
- Gameplay/reducer chronology remains authored.

No Build, Automation or PIE result is claimed by this design document.