# Card Selection Presentation Constraints

Date: **2026-09-09**

Status:

```text
PARTIALLY IMPLEMENTED / VISUAL-OWNERSHIP REDESIGN AUTHORITATIVE /
LIFECYCLE WATERMARK + OWNERSHIP DIRTY CONTRACT DEFINED /
G0 A/B/C IMPLEMENTED / AUTOMATED EVIDENCE RECORDED / MANUAL PIE PENDING /
GROUP PRODUCTION CODE NOT IMPLEMENTED / NOT SEALED
```

Scope: define the shared Native HUD / Presentation contract for current and future player card-selection interactions.

This document is authoritative for card-selection Presentation behavior. `docs/CardSelectionRefactorConstraints.md` remains authoritative for Gameplay candidate capture, pending requests, resolver/continuation behavior and interactive-boundary rules. `docs/SelectionPresentationGroupDesign.md` specifies the grouped multi-selection implementation in more detail.

Current implementation and evidence are recorded in `docs/SelectionPresentationG0Execution.md` and `docs/Validation.md`. G0 uses incremental Native dirty propagation and Battle-scoped Hand Widget reuse; its ownership APIs and empty SelectionAreaHost are dormant. Production Selection still uses formal-Hand transforms and `ConfirmedCardCenters`. Sections below describe the target unless explicitly identified as current; they do not authorize an early ownership switch.

## 1. Core principle

Selection Presentation belongs to the shared Selection interaction framework, never to an individual card or Effect.

It MUST NOT belong to Warcry, Burning Pact, `USelectHandCardToDrawPileTopEffect`, `USelectExhaustHandCardEffect`, or any other Effect-specific path.

The reusable interaction model is:

```text
Player Selection becomes display-eligible
→ shared Selection UI appears
→ player changes transient selected RuntimeIds
→ required count is satisfied
→ Confirm becomes available
→ player explicitly confirms
→ authoritative SelectionResult is submitted
→ confirmed selected RuntimeIds keep stable Presentation ownership
→ committed Gameplay/Presentation facts determine destination transitions
→ later authored Effects / played-card cleanup continue normally
```

No `CardId`-specific Presentation sequence is permitted.

## 2. Selection and confirmation are separate player intents

Choosing enough cards MUST NOT automatically submit the Selection.

```text
selected count < required count
→ Confirm disabled

selected count == required count
→ Confirm enabled
→ Selection remains pending

player presses Confirm
→ submit exactly the currently selected RuntimeIds
```

The last candidate click MUST NOT double as confirmation. Deselect/reselect remains transient UI behavior while the request is pending and policy permits it.

## 3. Authoritative visual continuity model

The primary selected-card continuity contract is exact RuntimeId Presentation ownership:

```text
Hand
→ SelectionArea
→ Transition
→ ConsumedPendingReducer
→ Done
```

This is Presentation-only state. It is not a Gameplay zone and MUST NOT mutate authoritative Hand/DrawPile/Discard/Exhaust membership.

A card may therefore legally be:

```text
Gameplay Zone       = Hand
Presentation Owner  = SelectionArea
```

### 3.1 One visible owner per RuntimeId

At most one visible Presentation surface owns a RuntimeId at any moment.

Conceptually:

```cpp
enum class ECardPresentationOwner : uint8
{
    Hand,
    SelectionArea,
    Transition,
    ConsumedPendingReducer
};
```

The exact implementation type may differ, but behavior must be equivalent.

### 3.2 Ownership lifecycle identity

RuntimeId identifies the card, but ownership also needs a selection lifecycle identity. Behavior must distinguish at least:

```text
BattleId
SelectionGeneration or equivalent monotonically unique selection lifecycle id
Selection boundary/revision identity
RuntimeId
Owner
Pending vs Confirmed phase
CompletionWatermark
```

A stale selection callback, prior battle, prior boundary or prior generation MUST NOT mutate a newer ownership entry merely because RuntimeId matches.

SelectionArea phases are distinct:

```text
SelectionArea + Pending
→ visible
→ selectable/deselectable

SelectionArea + Confirmed
→ visible
→ input disabled
→ waiting for committed/direct-state outcome or transition acceptance
```

GroupId may be associated later when known, but is not required to create SelectionArea ownership.

### 3.3 Confirmed lifecycle completion watermark

A Confirmed lifecycle MUST have a runtime-comparable completion watermark. The disappearance of the pending Selection request/resolver is explicitly **not** that watermark.

Normal happy path is:

```text
Confirm
→ request/resolver clears
→ continuation Gameplay executes
→ Presentation Envelope seals
→ Controller later plays/reconciles that continuation
```

Therefore restoring `SelectionArea(Confirmed) → Hand` merely because the request is no longer pending is forbidden.

Behavior must be equivalent to a tagged watermark with these modes:

```text
Recorded Presentation mode
    BattleId
    SelectionGeneration
    BoundaryRevision
    Owning/Continuation ResolutionId

Direct/no-history mode
    BattleId
    SelectionGeneration
    BoundaryRevision
    authoritative post-confirm StateRevision/baseline watermark
```

Recorded mode is reached only when the owning/continuation Resolution has been Presentation-completed, collapsed/reconciled to its FinalSnapshot, or otherwise formally completed by Controller policy.

Direct/no-history mode is reached only when the authoritative post-confirm baseline/revision is the displayed state.

A selected RuntimeId may still terminate earlier if displayed Hand state already proves it was consumed. The completion watermark is required specifically to decide the fallback case where the card remains in displayed Hand and Presentation must know that no later destination outcome can still arrive.

## 4. Shared SelectionArea lifecycle

### 4.1 Select

A legal local candidate click transfers visible ownership:

```text
Hand → SelectionArea
```

Gameplay remains unchanged.

The historical Hand slot remains structurally present, but its formal Widget is `Hidden` and cannot receive input while owner != Hand.

### 4.2 Deselect

While the request remains pending and policy allows it:

```text
SelectionArea → Hand
```

No Gameplay mutation occurs.

### 4.3 Confirm

Confirm freezes the selected RuntimeIds and changes their SelectionArea phase to Confirmed. It MUST NOT destroy the selected visible card objects merely because the interactive overlay closes.

```text
SelectionArea(Pending)
→ SelectionArea(Confirmed)
→ authoritative SelectionResult submit
```

Confirm also arms/associates the exact lifecycle with a completion watermark as soon as the relevant recorded Resolution or direct post-confirm state edge is knowable. The lifecycle may temporarily be `Confirmed + completion watermark not yet resolved`, but that state MUST NOT be treated as completed.

This is an acceptance transaction, not unconditional mutation before a fallible Request. Preserve the exact pending request/lifecycle and visuals during submit; commit Confirmed only for accepted submission, or use reversible preparation that cannot publish a false completion. Rejection with the same pending request restores Pending interaction; rejection due to a replaced/failed request reconciles against that new boundary. Detailed outcome correlation and re-entrancy requirements are in Group design sections 5.5 and 9.3 and gate G5.

### 4.4 Destination playback acceptance

Only an actually accepted visible transition transfers ownership:

```text
SelectionArea → Transition
```

The destination comes only from committed Presentation facts such as `CardZoneChanged`. The Effect does not choose or trigger animation directly.

### 4.5 Animation finishes before chronological reducer consumption

For a visually consumed future group member:

```text
Transition → ConsumedPendingReducer
```

No visible card remains, but Hand still has no right to render that RuntimeId while the chronological working snapshot still contains it.

### 4.6 Reducer/direct-state reconciliation terminates ownership

When the displayed frozen state proves the selected card has been consumed, the ownership entry is cleared independently of whether animation playback succeeded.

This is a non-negotiable rule:

> Presentation ownership must be reconciliable independently of successful animation playback. Animation may transfer visible ownership, but reducer/direct-baseline/final-snapshot reconciliation must always be able to terminate or restore ownership.

## 5. Ownership reconciliation and degradation recovery

Ownership MUST NOT rely solely on a destination animation callback for cleanup.

After each authoritative displayed frozen snapshot/baseline reconciliation, exact ownership entries are reconciled against that displayed state, lifecycle phase and completion watermark.

### 5.1 RuntimeId no longer exists in displayed Hand

If a Selection-owned RuntimeId no longer exists in the displayed historical Hand:

```text
destination/reducer/direct-state already consumed it
→ clear SelectionArea / Transition / ConsumedPendingReducer ownership
→ destroy/release any stale visible SelectionArea card
```

This covers:

- normal SingleRecord completion;
- grouped chronological reducer consumption;
- Widget decline followed by normal reducer advance;
- no-history/direct-baseline mode;
- FinalSnapshot reconciliation;
- Presentation skip/catch-up.

This rule does not need to wait for lifecycle completion watermark because the displayed historical state already proves the card is no longer in Hand.

### 5.2 RuntimeId remains in Hand after exact completion watermark is reached

Recovery to Hand is legal only when all are true:

```text
RuntimeId still exists in displayed Hand
AND ownership entry matches current BattleId + SelectionGeneration + boundary
AND Confirmed lifecycle completion watermark is resolved and reached
AND exact visual work is finished/cancelled at the completion boundary
```

Then:

```text
degradation recovery
→ owner = Hand
→ SelectionArea visual is released
→ formal Hand Widget becomes visible again
```

This prevents ghost SelectionArea cards for malformed/unsupported cases such as a selected object producing zero eligible destination records.

For G0-G7, a stale Transition/ConsumedPendingReducer entry must not veto this fail-safe: the reached exact watermark recovers all Confirmed non-Hand owners still in Hand. Cancel/release visual work before publishing completion, then reconcile ownership coherently. Recorded completion is exact Resolution membership, not `LatestCompletedResolutionId >= owningId`. Pending stale-boundary cleanup is separate from Confirmed completion.

The following MUST NOT count as lifecycle completion by themselves:

- pending request removed;
- resolver no longer pending;
- Confirm button/overlay closed;
- Selection input state cleared;
- group metadata missing/incomplete;
- one candidate produced no eligible destination Record before the owning Resolution has actually reached its completion watermark.

Recorded zero-member degradation therefore works as:

```text
Confirm
→ owner remains SelectionArea(Confirmed)
→ owning Resolution eventually reaches FinalSnapshot/completion watermark
→ RuntimeId still in Hand
→ exact visual work finished/cancelled at completion
→ owner returns to Hand
```

### 5.3 Widget decline

A Widget declining visible playback MUST NOT strand ownership:

```text
A owner = SelectionArea
→ SingleRecord Widget returns false
→ Controller reduces A normally
→ displayed Hand no longer contains A
→ ownership reconciliation clears A
```

If the declined/malformed path leaves A in Hand, restoration waits for the exact completion watermark from 5.2 rather than request disappearance.

### 5.4 No-history / direct baseline

Recording-disabled Selection remains mandatory Gameplay behavior. It may have no committed recorded destination boundary.

Therefore SelectionArea ownership MUST reconcile from the resulting direct/frozen baseline and MUST NOT wait forever for a Presentation Record that does not exist.

Direct-mode lifecycle completion is reached only when the authoritative post-confirm baseline/revision watermark is the displayed state:

```text
post-confirm direct baseline displayed
→ if RuntimeId absent from Hand: clear owner
→ if RuntimeId still in Hand after exact visual cleanup: owner = Hand
```

### 5.5 Ownership mutation has an independent dirty/event channel

Ownership changes frequently occur without a historical snapshot change and therefore MUST have an independent transient Presentation notification path.

Behavior must expose an event/change descriptor equivalent to:

```text
OnCardPresentationOwnershipChanged
or
TransientPresentationDirty(CardOwnership, ChangedRuntimeIds...)
```

The exact API/type is flexible, but these transitions must publish ownership dirty state immediately:

```text
Hand → SelectionArea(Pending)       // select
SelectionArea → Hand                // deselect
Pending → Confirmed                 // Confirm
SelectionArea → Transition          // playback accepted
Transition → ConsumedPendingReducer // grouped visual completion
```

Ownership state may live in `UBattleHUDViewModel` or in a dedicated transient Presentation state object owned beside it, but the lifecycle is separate from historical snapshot copying:

- `FPresentationStateSnapshot` does not contain or overwrite transient card Presentation ownership;
- normal `ApplyPresentationSnapshot()` MUST NOT clear/reset ownership simply because historical values/revision changed;
- snapshot application copies historical display state first, then runs ownership reconciliation against the new displayed state and completion watermarks;
- ownership reconciliation emits its own ownership dirty notification;
- if historical and ownership changes occur in one operation, notifications must be coherent/batched so no transient duplicate-visible or ghost frame is exposed.

Formal Hand visibility/input and SelectionArea visuals must react to ownership dirty changes even when `HandCards` itself is unchanged.

## 6. SelectionArea Host contract

The production SelectionArea is a persistent Presentation surface, not a transform offset inside `HB_Hand`.

Initial implementation contract:

```text
SelectionAreaHost
- runtime-created persistent Overlay/Canvas-compatible layer
- lifetime owned by BattleHUDSelectionWidget / Native HUD instance
- NOT HB_Hand
- NOT OV_PlayArea
- survives RefreshHand / Hand reconciliation
- survives closing the interactive Selection overlay after Confirm
- uses stable HUD coordinate space
```

The Host itself may remain allocated while empty.

Interaction rules:

```text
Pending selected visual
→ hit-testable only as required for deselect/reselect

Confirmed selected visual
→ visible but input disabled / HitTestInvisible

Transition accepted
→ generic transition presenter takes/reparents or transactionally copies the exact visual
```

No production `.uasset` change is required merely to provide this runtime Host.

## 7. Formal Hand structural contract

Historical Hand presentation preserves exact frozen Hand child count and index correspondence while index-based historical contracts remain in production.

If a RuntimeId is still present in `WorkingPresentationSnapshot.HandCards` but `PresentationOwner != Hand`:

```text
one frozen Hand entry
↔ one formal HB_Hand child at the same index
Visibility = Hidden
Input = disabled
```

The formal child MUST NOT be omitted, removed, or `Collapsed` while the historical Hand entry still exists.

Important distinction:

```text
formal Hand Widget
= historical structural slot

SelectionArea visual
= visible Presentation owner
```

The formal Hand rebuild/reconcile MUST NOT reconstruct SelectionArea continuity by restoring confirmed transforms or positions.

Formal Widget reuse is Battle-scoped. Completed draw adoption must restore normal hit testing/request binding before applying explicit ownership suppression. A temporary draw visual may be appended during active playback; the exact child-count invariant applies at stable snapshot/preflight boundaries, not midway through that existing animation.

Legacy `ConfirmedCardCenters`, position handoff data or equivalent may exist temporarily only as migration compatibility implementation detail. They are **not** a second authoritative ownership mechanism and MUST be removed after the production ownership migration is validated.

## 8. Historical index vs visual identity

Committed Hand indexes remain valid historical facts and may validate the frozen snapshot/order.

They MUST NOT be the primary live visual identity.

```text
Historical index
→ validates committed historical sequence

RuntimeId
→ identifies the exact presentation object/ownership entry
```

A valid state may be:

```text
WorkingSnapshot.HandCards[2].RuntimeId == A
PresentationOwner(A) == SelectionArea
```

Generic destination playback resolves the current visual owner by RuntimeId rather than assuming `HB_Hand.Child[FromIndex]` is the visible source.

## 9. Destination transitions are generic

Selection Presentation dispatches the exact selected RuntimeId according to committed zone facts.

Examples:

```text
Hand/SelectionArea → DrawPile
→ visible movement to DrawPile anchor

Hand/SelectionArea → DiscardPile
→ generic discard transition

Hand/SelectionArea → ExhaustPile
→ generic Exhaust transition
```

No Warcry/Burning-Pact/Effect-specific animation branches are permitted.

### 9.1 Hand/SelectionArea → DrawPile contract

For:

```text
CardZoneChanged
FromZone = Hand
ToZone = DrawPile
RuntimeId = X
```

Native Presentation MUST:

1. resolve the exact current visible owner of RuntimeId X;
2. transfer that exact visual into the generic transition presenter;
3. visibly move it toward the DrawPile anchor;
4. preserve exact card identity during flight;
5. reconcile displayed Hand/DrawCount from committed facts.

It MUST NOT use fade-only as the primary DrawPile transition, replay DrawPile→Hand, reuse Discard/Exhaust semantics, mutate Gameplay, or contain Warcry-specific logic.

## 10. Multi-selection ordering and explicit Group exception

Gameplay mutation, event dispatch, committed `PresentationSequence` and reducer application remain canonical authored order.

```text
Reducer order                    = always PresentationSequence order
Normal visible playback order    = PresentationSequence order
Validated explicit group members = may co-present together
```

A complete explicit Selection Presentation Group may co-present its own sealed-envelope members, including non-contiguous members separated by unrelated trigger records.

Group lookahead MUST NOT:

- reduce future members early;
- consume/skip interleaved ungrouped Records;
- include untagged Records;
- infer membership from CardId, Effect, destination, adjacency, timing or click order;
- expose mutable future Gameplay.

`ExpectedMemberCount <= 1` remains normal SingleRecord Controller playback initially.

Expected count alone is not proof of selected membership. G1 must provide a sealed canonical selected-identity manifest (or equivalent writer-validated evidence), including zero-record outcomes; G2 compares exact membership and checks all interleaved records, including members of other groups. See Group design sections 12 and 17.

## 11. Controller semantic preflight vs Widget visual preflight

The Controller deals only with immutable committed semantics. It MUST NOT query or mutate concrete HUD ownership state.

### 11.1 Controller semantic preflight

Controller may validate only sealed-envelope facts such as:

- group identity/metadata;
- expected member count;
- record type/zone shape;
- unique RuntimeIds and `PresentationSequence` values;
- exact-one-eligible-record-per-selected-member invariant;
- chronological reducer dry-run;
- future-member interference;
- terminal/record validity.

### 11.2 Widget transactional visual preflight

`PlayPresentationGroup(...)` performs visual acceptance using Presentation-owned state:

- exact SelectionArea/current visual owner exists where required;
- exact visible object is valid;
- source geometry is valid;
- destination anchor is valid;
- every child can be constructed;
- no ownership is transferred during partial preparation.

Any visual preflight failure returns false and Controller degrades to sequential playback.

Controller MUST NOT cast/query concrete Selection Widget ownership to decide group semantic eligibility.

### 11.3 Group completion ownership

Concrete Presentation/UI state performs:

```text
Transition → ConsumedPendingReducer
```

for visually consumed future members before forwarding exact group completion through the hardened Base Widget callback.

Controller records only which record indices were visually presented. It does not directly set/clear card visual ownership.

## 12. Future-member interference

Chronological reducer dry-run is necessary but insufficient.

For each interleaved Record outside the candidate group before a future member's own reducer position, a direct exact-card operation on that future RuntimeId disables group co-presentation. A Record tagged for another group is not exempt.

At minimum:

```text
CardZoneChanged touching future RuntimeId → interference
CardPlayed touching future RuntimeId      → interference
```

Future exact-card record types must join this classification or conservatively disable lookahead until classified.

Example:

```text
A Hand→Exhaust [G]
B Hand→Discard [ungrouped]
B Discard→Hand [ungrouped]
B Hand→Exhaust [G]
```

Even if reducer dry-run succeeds, Group G degrades to sequential playback.

Unrelated Damage/Status/Energy/other-card records do not disable grouping merely because they are interleaved.

## 13. Group transactional visual transfer

Group visual preparation is all-or-nothing.

Before accepting a Group:

1. prepare every exact child;
2. validate every current source visual and destination;
3. do not release SelectionArea ownership during partial preparation.

If child N fails:

```text
rollback all prepared children
→ every member remains owned by SelectionArea
→ zero member becomes VisuallyPresented
→ sequential fallback
```

Only after the whole Group is accepted may all member owners transfer `SelectionArea → Transition` in one transaction and begin together.

## 14. ConsumedPendingReducer continuity

After grouped A/B/C destination animations finish together:

```text
A owner = ConsumedPendingReducer
B owner = ConsumedPendingReducer
C owner = ConsumedPendingReducer
```

No visible selected visual remains. Historically still-present B/C formal Hand children remain `Hidden` because owner != Hand.

When the chronological reducer later consumes B and the displayed snapshot no longer contains B in Hand, ownership reconciliation removes B's entry.

This replaces ad-hoc independent `SuppressedRuntimeIds` as the primary contract. Any cached suppression representation must be derived from, or behaviorally equivalent to, exact ownership state and cannot become a second source of truth.

## 15. Played-card cleanup is independent

The played card and selected cards are separate Presentation responsibilities.

For Warcry-style resolution:

```text
Warcry in PlayArea
→ Draw Presentation
→ Selection UI / SelectionArea
→ explicit Confirm
→ selected card SelectionArea→DrawPile generic transition
→ Warcry later uses ordinary PlayArea→Exhaust cleanup
```

No dedicated Warcry fade/Exhaust animation is allowed.

## 16. Gameplay / Presentation separation and failure reconciliation

Animation callbacks MUST NOT mutate authoritative Gameplay zones.

Presentation failure/skip/catch-up may reconcile visual ownership and displayed frozen state, but MUST NOT invent a different Gameplay result.

### 16.1 Group timeout is not normal completion

A timed-out Group MUST NOT leader-complete through `CompleteActiveRecord()`.

Required flow:

```text
validate exact Group token
→ cancel exact Base-Widget playback unit
→ clean every group child
→ do not mark failed members VisuallyPresented
→ terminate transient ownership belonging to failed active envelope
→ atomically reconcile ActiveEnvelope to ActiveEnvelope.FinalSnapshot
→ reconcile card Presentation ownership from that final snapshot
→ mark current envelope Presentation-complete
→ preserve later queued Envelopes
→ continue backlog
```

No intermediate HUD publication may expose a selected historical card between ownership cleanup and FinalSnapshot application.

### 16.2 Skip / Widget replacement / PresentationUnavailable

These boundaries must either restore or terminate ownership through displayed-state reconciliation. No stale SelectionArea visual may survive battle replacement, stale selection generation, Widget loss, global skip/collapse or direct-baseline fallback.

## 17. Interaction boundary and input-event consumption

Selection UI starts only after the existing interactive Presentation boundary makes the request display-eligible.

One pointer/input event MUST NOT cross interaction-state boundaries.

```text
input fast-forwards prior Presentation
→ Selection becomes visible
→ input ends
→ later input may select
```

Likewise the candidate click that reaches required count only enables Confirm; a later explicit Confirm submits.

## 18. Effect contract

An Effect may configure Gameplay selection intent only:

- candidate source;
- Player/Random mode where supported;
- count/count policy;
- cancellation policy;
- semantic source/prompt data;
- authored continuation.

It MUST NOT own Confirm behavior, SelectionArea lifetime, destination animation, played-card cleanup, group playback or input-event consumption.

## 19. C1 / Warcry acceptance flow

Required observable flow:

```text
Warcry played
→ Draw Gameplay commit
→ Draw Presentation
→ post-Draw Hand becomes selectable
→ shared Selection UI
→ player selects required card
→ explicit Confirm
→ selected RuntimeId remains SelectionArea-owned after overlay closes
→ SingleRecord generic transition takes SelectionArea visual
→ visible SelectionArea→DrawPile movement
→ ownership reconciles when destination state is displayed
→ Warcry ordinary generic PlayArea→Exhaust
→ resolution completes
```

Warcry remains a one-member SingleRecord Controller case while reusing the same generic transition child engine as grouped playback.

## 20. Implementation staging contract

Every landed stage must preserve production visual behavior, not merely compile.

Required order:

```text
G0-A incremental ViewModel/HUD dirty propagation
G0-B RuntimeId-keyed Hand reconcile
G0-C ownership infrastructure + lifecycle identity + completion watermark
     + independent ownership dirty/event channel + reconciliation
     + persistent SelectionAreaHost
     (dormant/compatibility only; do not switch production source ownership yet)
G1   PresentationGroup metadata + writer-scoped correlation
G2   Controller semantic group discovery / reducer dry-run / interference
     (Group visible playback still disabled)
G3   Base Widget Record-or-Group playback hardening + recovery scopes
G4   generic card transition engine; source resolver supports Hand | SelectionArea;
     migrate SingleRecord production paths first
G5   switch production Selection to durable SelectionArea ownership;
     retire confirmed-center handoff as primary path
G6   N-child Group playback + ConsumedPendingReducer + parallel enable
G7   delete compatibility handoff/old Selection-specific transition code;
     focused validation and seal work
```

G0-C/G5 separation is mandatory unless combined into one coherent behavior-safe migration. It is forbidden to hide the formal Hand source in production before the generic SingleRecord transition engine can consume a SelectionArea source.

G0 is already implemented; resume from its recorded evidence and remaining manual draw/selection check. G1 must establish exact Selection-to-continuation outcome correlation independently of optional Group tags; G3 wires exact completion and scoped recovery; both are prerequisites for G5. First close correct sequential SelectionArea playback, then enable Group concurrency. G8 remains deferred and requires the explicit lifetime amendment in Group design section 30.5.

## 21. Acceptance gates

Implementation is not complete until focused tests and PIE demonstrate at minimum:

- explicit Confirm semantics remain correct;
- selecting/deselecting transfers exact RuntimeId visible ownership Hand↔SelectionArea without Gameplay mutation;
- Pending vs Confirmed SelectionArea phases have correct input behavior;
- request/resolver disappearance immediately after Confirm does **not** restore owner to Hand;
- recorded completion watermark is reached only after the owning Resolution is Presentation-complete/collapsed/reconciled;
- direct/no-history completion watermark is reached only after the post-confirm authoritative baseline is displayed;
- zero eligible destination record restores to Hand only after its exact lifecycle watermark is reached;
- SelectionAreaHost survives Hand reconciliation and overlay close after Confirm;
- formal Hand child count/index remains historical; owner!=Hand uses `Hidden`, never `Collapsed`;
- Hand reconciliation does not reconstruct SelectionArea continuity through confirmed transforms;
- ownership mutations update Hand visibility/SelectionArea immediately without requiring a new historical snapshot;
- `ApplyPresentationSnapshot()` does not overwrite transient ownership and runs reconciliation after historical copy;
- historical dirty + ownership dirty can publish coherently without a duplicate-visible/ghost frame;
- Widget decline followed by reducer advance clears ownership with no ghost SelectionArea card;
- no-history/direct-baseline Selection clears/restores ownership correctly;
- selected object with zero eligible destination record cannot remain SelectionArea-owned forever;
- Controller group semantic preflight never queries concrete Widget ownership;
- Widget group visual preflight is transactional; child-N failure transfers zero ownership;
- valid multi-member group starts all destination children together;
- non-contiguous unrelated interleaving is allowed; exact future-member interference disables grouping;
- visually consumed future members remain `ConsumedPendingReducer`/hidden until exact historical consumption;
- sequential degradation leaves later confirmed members visibly in SelectionArea rather than flashing to Hand;
- Group timeout reconciles only the active envelope and preserves later backlog;
- Record/Group callbacks share exact-token/deferred-completion/cancellation hardening;
- Warcry SingleRecord consumes SelectionArea source through the generic transition engine;
- Warcry final cleanup remains ordinary PlayArea→Exhaust;
- no Effect/CardId-specific animation branch exists;
- Gameplay remains authoritative through skip/degradation.

No Build, Automation or PIE gate may be marked PASS without actual execution evidence.

## 22. Historical pre-redesign implementation note — 2026-09-08

The production Selection implementation preceding this redesign used the formal Hand Widget itself as the selected visible card, moved it with render translation, captured confirmed centers by RuntimeId, and used a Selection-subclass Hand→DrawPile transfer. Hand→Exhaust could reuse the first selected formal Widget's confirmed transform, but later serial members could lose that transform when `RefreshHand()` rebuilt the formal Hand.

That behavior explains the current redesign but is **not** an authoritative future continuity contract. In particular:

```text
ConfirmedCardCenters
position-only handoff lease
RefreshHand restore confirmed transform
```

are legacy migration details only.

Previously obtained build/Automation/PIE evidence applies only to the code state that produced it. It does not validate the SelectionArea ownership or parallel Group redesign.
