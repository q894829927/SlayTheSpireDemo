# Selection Presentation G9 — Buffered Card Input + Detached Card Presentation

Date: **2026-09-12**

Status: **DESIGN REVIEWED / NOT IMPLEMENTED / NOT SEALED**

Authority baseline: [`SelectionPresentationG8FSeal.md`](SelectionPresentationG8FSeal.md).

G8 is sealed. G9 is a new initiative and must not reopen sealed G8 behavior for speculative cleanup.
This document defines the reviewed scope, sequencing, ownership rules, stale fencing, migration strategy,
and acceptance criteria for buffered next-card **selection** and detached played-card visual tails. It does
**not** authorize production implementation by itself.

This revision freezes the following architecture decisions before G9 implementation begins:

```text
1. ExpectedReadyRevision is Controller-authoritative and must identify an already-sealed normal player surface; it is never predicted.
2. A buffered card click replays card selection only. It never replays Confirm or Target input.
3. Buffered-intent ownership and replay are event/readiness driven, never cosmetic-tick driven.
4. A second physical click re-captures its exact credential; RuntimeId-only replacement is allowed only when the credential is identical.
5. Card presentation gets a Controller-owned played-card lifecycle identity separate from RuntimeId and separate from visual-job identity.
6. Detached card jobs own all per-instance animation phase/geometry/elapsed state; no singleton animation state may be shared across jobs.
7. Detached card visuals live in a dedicated non-interactive host and never occupy formal OV_PlayArea / Hand ownership surfaces.
8. Card historical reducer semantics are centralized before NonBlocking activation; visual eligibility is a separate concern.
9. G9-D is split into D1 destination-tail detach first, then D2 CardPlayed-arrival detach.
10. D2 CardPlayed detach is allowed only when the matching future PlayArea destination lifecycle is already sealed and exactly correlatable.
11. Other committed Records may exist between the card-visual anchor and the exact target player surface; buffering does not imply Skip.
12. Feature disable always clears buffered intent. Already-committed card lifecycle correlation remains long enough to finish formal chronology safely.
13. Hand hover is split from structural layout, and non-Hand ViewModel updates must not needlessly rebuild the formal Hand.
```

## 1. Goal

G9 targets the following player experience:

```text
play card A
→ A card Presentation continues

while A visual is still alive:
→ surviving formal Hand card B can hover / raise normally
→ clicking B does not Skip A merely because A is still visible

if the exact normal player-card surface is not displayed yet:
→ capture one buffered selection intent for B
→ do not execute B Gameplay early

when the exact sealed player-card surface is reached:
→ revalidate Battle / Session / revision / RuntimeId / legality
→ replay B selection exactly once if still valid
```

Important interaction semantics:

```text
buffered click B
→ later replays SelectCard(B) only

if B has no target:
→ enters ReadyToConfirm
→ requires a NEW physical Confirm input

if B requires a target:
→ enters ChoosingTarget
→ requires a NEW physical Target input
```

The old click must never cross two interaction-state transitions. G9 does not convert the current
selection/confirm/target interaction model into click-to-autoplay.

After the fresh Confirm/Target input is accepted, B Gameplay and B Presentation may begin while A's old
cosmetic card tail is still alive.

Core invariant:

> **G9 may overlap card visual lifetimes, but it must not overlap authoritative Gameplay resolutions.**

The authoritative flow remains:

```text
physical input
→ exact interaction request
→ BattleActionQueue / reducer / committed Presentation facts
→ exact player-facing read boundary
→ next legal physical/request boundary
```

Visual overlap is never permission to speculatively execute future Gameplay.

## 2. Current baseline and why G9 needs separate architecture

### 2.1 FastInput currently means catch-up by Skip

The G8-sealed FastInput path captures:

```text
SessionToken + BattleId + ExpectedCatchUpRevision
```

and, when real Blocking chronology owns the delay, performs:

```text
click
→ SkipPresentation()
→ next-tick exact retry
```

That is correct for explicit catch-up. It is not a buffered-input scheduler and must remain sealed.

G9 therefore introduces a distinct buffered-selection contract whose normal path is:

```text
click
→ capture exact intent
→ DO NOT Skip
→ wait for exact already-known target surface
→ replay selection once
```

### 2.2 Hand hover currently mixes structural and affordance work

Current Native Hand interaction is gated while tracked/native Presentation is active. In addition,
`UBattleHandFanPanel::UpdateInteraction()` performs layout work before hover transforms, and the HUD's
normal ViewModel refresh path can rebuild the formal Hand even when a publication changed only unrelated
surfaces.

G9 must separate:

```text
A. formal Hand structural reconciliation
   card set / slot order / authoritative membership / base geometry

B. hover affordance
   visual-only raise / scale / angle / z-order on surviving formal Hand cards
```

A non-Hand publication must not destroy/recreate the Hand merely to refresh HP, Status, Energy or another
unrelated surface. Equivalent dirty-aware/stable-widget behavior is required before G9-B can claim hover
continuity.

### 2.3 Card lifecycle currently has singleton cross-record ownership

The sealed R8 lifecycle intentionally retains one `NativePlayedCardWidget` from `CardPlayed` finish until
the matching `PlayArea -> destination` Record.

The Native card animation also stores singleton state such as active moving Widget, phase kind, elapsed
time, start/end geometry, scale and opacity.

Therefore this migration is insufficient:

```text
NativePlayedCardWidget
→ TArray<Widget>
```

G9 needs true per-lifecycle/per-job ownership before two old/new card visuals can coexist.

### 2.4 Formal PlayArea and detached card visuals cannot share a container

Current R8 historical checks assume the formal `OV_PlayArea` has the exact expected child shape for a new
`CardPlayed` Record. An old A tail left inside that container would make B CardPlayed fail historical/visual
preconditions.

G9 therefore freezes a dedicated detached-card visual host. Old tails must not consume formal PlayArea or
formal Hand child ownership.

### 2.5 G8 authority remains unchanged

G9 reuses the sealed G8 authority distinction:

```text
PresentationOwned
→ valid Controller + exact PresentationSessionToken

DirectBaseline
→ intentional sessionless direct delivery
```

G9 buffered Presentation lag exists only in `PresentationOwned` mode. DirectBaseline has no Controller-owned
card chronology to buffer through and does not receive a fake SessionToken.

## 3. Scope

### 3.1 G9 v1 card Presentation scope

The reviewed v1 target includes:

```text
CardPlayed
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Activation is intentionally staged:

```text
G9-D1
→ detach PlayArea -> destination tail first
→ CardPlayed arrival remains Blocking

G9-D2
→ detach CardPlayed arrival only after D1 is proven
→ support destination command arriving before visual arrival finishes
```

Both D1 and D2 belong to the full G9 v1 scope unless a later explicit scope amendment removes D2.

### 3.2 Remain Blocking in G9 v1

Unless separately amended, these remain Blocking:

```text
DrawPile -> Hand
Hand -> Discard at turn cleanup
Hand -> Exhaust outside the played-card lifecycle
SelectionArea transitions
G6 multi-selection Group
Shuffle
PendingSelection presentation
TargetChoice presentation
formal Energy / HP / Block / Status updates
Terminal / unavailable / recovery
```

G9 is not a universal Presentation concurrency framework.

### 3.3 Buffered intent scope

First version buffers **one card-selection intent only**.

It does not buffer:

```text
EndTurn
Confirm
Cancel
Target click
PendingSelection submit/cancel
multiple future card commands
```

A buffered target-requiring card may later enter `ChoosingTarget`, but the target requires a fresh physical
click. A buffered no-target card may later enter `ReadyToConfirm`, but Confirm also requires a fresh click.

## 4. Buffered card intent authority

### 4.1 Separate target credential, capture-window identity and requested RuntimeId

Conceptually the buffered state is:

```cpp
struct FBufferedCardTargetCredential
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    int64 ExpectedReadyRevision = 0;
};

struct FBufferedCardWindowIdentity
{
    // Exact provenance of the card Presentation window in which the physical click occurred.
    int64 SourceResolutionId = 0;
    int64 SourcePresentationSequence = 0;
    uint64 LocalWindowGeneration = 0;
    // Implementation may additionally encode Blocking-playback vs detached-job anchor kind.
};

struct FBufferedCardIntent
{
    FBufferedCardTargetCredential Target;
    FBufferedCardWindowIdentity CaptureWindow;
    int32 RuntimeId = INDEX_NONE;
};
```

Exact type names/layout are not frozen, but the three responsibilities are:

```text
Target credential
→ authoritative future replay surface

Capture-window identity
→ proves whether two physical clicks happened in the same exact bufferable visual window

RuntimeId
→ requested authoritative card instance
```

`RuntimeId` is not a visual-job identity and cannot replace `CaptureWindow`.

### 4.2 ExpectedReadyRevision is minted by Controller authority only

G9 must not predict a future revision and must not simply rename the current FastInput target capture.

A Controller-authoritative helper conceptually provides:

```cpp
bool TryCaptureBufferedCardTarget(
    int32 RequestedRuntimeId,
    FBufferedCardTargetCredential& OutCredential) const;
```

It may succeed only when all of the following are already true:

```text
PresentationOwned mode is current
exact current PresentationSessionToken exists
BattleId matches current Controller/ViewModel authority
latest frozen Presentation baseline already exists
latest frozen baseline is the exact intended target revision
latest target outcome == None
latest target BattleState is PlayerTurn
exact player-facing read can be built for the SAME BattleId + StateRevision
no authoritative PendingCardSelection owns that target boundary
RequestedRuntimeId exists as the same authoritative card instance in the target Hand
that target card is known player-facing/playable enough to justify buffering
no unavailable / terminal / recovery ambiguity owns the capture
```

The target is therefore **already sealed authoritative state whose display is lagging**, not a guessed state
that Gameplay may or may not reach.

If the exact target player read does not exist yet, G9 buffering declines. It does not invent:

```text
CurrentRevision + 1
ActiveEnvelope.FinalStateRevision without validating the exact player-facing target
latest integer revision regardless of mode
```

### 4.3 Target remains exact while waiting

While a buffered intent is waiting, a not-yet-displayed target is allowed to remain pending only while the
Controller/Battle can still prove the same frozen target credential is current.

Conceptually:

```text
ViewModel.StateRevision != ExpectedReadyRevision
+
latest sealed target is still exactly ExpectedReadyRevision
+
same Session/Battle authority
→ keep waiting

latest sealed target changes to another revision
→ buffered intent stale; drop
```

Do not use numeric ordering such as `<` or `>=` as authority. Revision numbers are exact identities, not
permission ranges.

### 4.4 Exact replay rule

Replay is permitted only when all of the following hold at one exact boundary:

```text
same exact PresentationSessionToken
same BattleId
latest frozen baseline is still ExpectedReadyRevision
current displayed ViewModel StateRevision == ExpectedReadyRevision
exact player-facing read == BattleId + ExpectedReadyRevision
Outcome == None
InteractionState == Idle / normal player-card selection surface
bInputLocked == false
no authoritative PendingCardSelection
RuntimeId exists in current displayed Hand
RuntimeId resolves to the same live authoritative card instance
normal Gameplay QueryCardPlayability passes now
no newer decision surface superseded the intent
```

Then and only then:

```text
replay SelectCardByRuntimeId(RuntimeId) once
```

The replay does not call Confirm, Target, EndTurn or Skip.

### 4.5 Buffered-intent owner and replay trigger

G9 v1 uses one UI-side owner for at most one `FBufferedCardIntent`. The exact class may be HUD-owned or an
equivalent battle-HUD input owner, but ownership must not be duplicated between HUD, ViewModel and Controller.

The Controller owns **target credential minting**. The ViewModel/Battle own **current readiness and Gameplay
legality**. The buffered-input owner owns only the pending physical intent.

Replay evaluation is triggered by authoritative Presentation/ViewModel/readiness transitions, for example
a dedicated readiness notification or an equivalent ViewModel-change hook.

It must **not** be driven by:

```text
NativeTick polling
card visual completion callback alone
DamageNumber completion
DetachedCardVisualJob completion
arbitrary timer expiry
```

### 4.6 Atomic consume before normal selection request

To prevent synchronous ViewModel broadcasts/reentrancy from replaying the same click twice:

```text
exact ready + exact revalidation
→ move/take buffered intent to a local value
→ clear stored buffered intent FIRST
→ call the existing normal SelectCardByRuntimeId path
→ never restore the old intent automatically
```

If normal selection is rejected by the final Gameplay query, the consumed old click remains consumed.

### 4.7 Stale fencing / mandatory clear boundaries

The buffered intent is cleared on:

```text
PresentationSessionToken replacement
HUD / Controller / battle replacement
BattleId change
authority transition
DirectBaseline transition
presentation unavailable
terminal transition
new authoritative PendingSelection boundary
ordinary Global Skip / backlog collapse
active-envelope recovery/reconcile involving its target chronology
runtime G9 disable
latest sealed target revision changing away from ExpectedReadyRevision
new physical input choosing a different non-buffer path
```

Card leaving Hand or becoming unplayable at replay also consumes/drops the intent.

Clearing a buffered intent never triggers Skip and never emits Gameplay.

### 4.8 Repeated physical clicks

Every physical card click must re-evaluate the current capture opportunity.

If the new click produces the exact same:

```text
TargetCredential
+
CaptureWindowIdentity
```

then the implementation may replace only the buffered `RuntimeId`.

If the new click produces a different valid credential, the new physical click may replace the **entire**
old intent with the new credential + RuntimeId.

Never bind a newly clicked RuntimeId to an older credential merely because a buffer already exists.

If the new click follows another route, such as sealed FastInput catch-up, clear the old G9 buffer before
starting that route so two different physical intents cannot later fire.

## 5. Buffered-input window classification

The presence of Presentation is not blanket permission to buffer.

### Case A — exact normal surface already ready

```text
normal card selection is legal now
→ clear any stale prior G9 buffer
→ SelectCard normally
→ no buffering
```

### Case B1 — Blocking card-presentation anchor

Used by G9-B and still available later when appropriate:

```text
current exact visible/tracked card playback is an approved G9 card anchor
+
Controller can mint an exact sealed normal-player TargetCredential
→ capture buffered card selection
→ do not Skip
```

Approved first-version Blocking anchors are limited to the played-card lifecycle being migrated; this is not
"any active Record".

### Case B2 — detached-card visual anchor

Enabled only after the corresponding detached-card job infrastructure is validated:

```text
an exact current-session detached card job from the relevant played-card lifecycle is still alive
+
Controller can mint the exact sealed normal-player TargetCredential
→ capture buffered selection
→ do not Skip
```

This allows an old A card visual to remain the UX anchor while Controller chronology continues through other
committed Records such as Damage or Status before the target player surface is displayed.

Those intermediate Records are allowed. Buffering does not require the card visual to be the **only**
remaining chronology.

The contract is:

```text
card visual = buffer capture anchor
sealed normal-player baseline = replay authority
intermediate chronology = must finish normally unless separately skipped by a NEW physical input route
```

### Case C — no G9 anchor / ambiguous or unsafe authority

Examples:

```text
PendingSelection owns the request
terminal / unavailable
recovery ambiguity
DirectBaseline
no exact sealed normal-player target
non-G9 Presentation with no active exact G9 card anchor
```

Do not create a buffered intent.

After clearing any conflicting old buffer, preserve sealed FastInput behavior if the **new** physical click is
FastInput-eligible; otherwise preserve the normal rejection/request path.

## 6. Hand structural ownership vs hover affordance

### 6.1 Structural Hand reconciliation

Structural work includes:

```text
authoritative Hand card membership
slot/order changes
base fan layout
formal card Widget creation/removal
arrival geometry used by committed Presentation
```

It remains governed by reducer/Presentation ownership.

G9-B must avoid unconditional formal Hand rebuilds for ViewModel publications whose dirty state does not
include a Hand change. Equivalent stable-widget reconciliation is acceptable.

When Hand really changes, authoritative reconciliation wins and may reset hover state safely.

### 6.2 Hover affordance

Hover work includes only presentation transforms on surviving formal Hand cards:

```text
hover hit region
raise translation
scale
angle interpolation
z-order
```

A conceptually split API is preferred:

```cpp
FanHand->ReconcileLayout(...);        // structural, only when required
FanHand->UpdateHoverAffordance(...);  // hover-only, no LayoutCards side effect
```

Exact names are not frozen.

Hover updates must not:

```text
call structural reconciliation implicitly
reorder formal Hand
move the active transient/detached card
claim a card owned by PlayArea or a detached job
write Gameplay selected-card state
mutate reducer state
restore a hidden historical source card
invalidate frozen transition geometry
```

Only currently visible/enabled formal Hand cards can be hover candidates. The historical source card hidden
for A CardPlayed is not a hover target.

## 7. Played-card lifecycle identity and detached visual ownership

G9 separates three identities:

```text
Gameplay card identity
→ RuntimeId

played-card Presentation lifecycle occurrence
→ Controller-owned FPlayedCardPresentationLifecycleToken

private visual instance
→ FDetachedCardVisualToken
```

They are not interchangeable.

### 7.1 Controller-owned played-card lifecycle token

Conceptual shape:

```cpp
struct FPlayedCardPresentationLifecycleToken
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    int64 SourceResolutionId = 0;
    int64 CardPlayedPresentationSequence = 0;
    uint64 LocalLifecycleGeneration = 0;
    int32 RuntimeId = INDEX_NONE;
    FName CardId = NAME_None;
};
```

This is Presentation correlation, not Gameplay zone ownership.

The Controller creates one exact unresolved lifecycle when a `CardPlayed` Record formally commits. The
matching `PlayArea -> destination` Record must consume that exact unresolved lifecycle.

Contract:

```text
at most one unresolved played-card lifecycle per RuntimeId
PlayArea destination requires exact current lifecycle correlation
zero or multiple matching unresolved lifecycles = historical/recovery failure
lifecycle correlation survives visual-job loss
lifecycle correlation clears when destination formally commits
Skip/recovery/replacement clears correlations whose chronology was collapsed/abandoned
```

After destination formal commit, an old visual tail may remain alive, but it no longer owns the unresolved
played-card lifecycle. Therefore the same RuntimeId may later return to Hand and begin a new lifecycle with a
new LocalLifecycleGeneration while the old cosmetic tail still exists.

### 7.2 Detached card visual token

Conceptually:

```text
DetachedCardVisualToken
=
  PlayedCardPresentationLifecycleToken
  + LocalVisualJobGeneration
```

The visual token must also retain source Record provenance needed to reject stale callbacks. RuntimeId alone
can never address a job.

### 7.3 Per-job state is complete, not just a Widget pointer

Conceptual job:

```cpp
struct FDetachedCardVisualJob
{
    FDetachedCardVisualToken Token;
    TObjectPtr<UBattleCardWidget> Widget;

    ECardVisualPhase Phase;
    float PhaseElapsedSeconds = 0.0f;
    float PhaseDurationSeconds = 0.0f;

    // Frozen host-local visual state.
    FVector2D StartPosition;
    FVector2D PlayAreaPosition;
    FVector2D DestinationPosition;
    float StartScale = 1.0f;
    float EndScale = 1.0f;
    float StartOpacity = 1.0f;
    float EndOpacity = 1.0f;

    bool bDestinationCommitted = false;
    // Optional exact frozen destination spec / destination Record provenance.
};
```

Every field that is currently singleton active-card animation state and is required for overlapping visuals
must move into the exact job or an equivalent per-job structure.

A detached job must never read/write another job's:

```text
elapsed time
anchors
phase
opacity
transform
completion state
Widget pointer
```

### 7.4 Dedicated `DetachedCardVFXHost`

G9 creates a runtime non-interactive visual host under the stable root Canvas, conceptually:

```text
DetachedCardVFXHost
```

Requirements:

```text
HitTestInvisible
never receives focus
never binds card request delegates
never participates in Hand/Selection/Target hit testing
never counts as formal OV_PlayArea child ownership
```

Before a card becomes detached, its start/play-area/destination geometry is converted to frozen host-local
coordinates using valid cached geometry.

If the viewport/DPI/root geometry changes such that safe remapping cannot be proven, cancel affected private
visual jobs. Formal state remains untouched.

### 7.5 Card visual phase machine

The reviewed phase model must support a destination command arriving before the arrival visual finishes:

```text
Prepared
→ EnteringPlayArea
→ AtPlayArea
→ DestinationTail
→ Done
```

with an orthogonal exact pending destination command:

```text
Destination formal Record commits while Phase == EnteringPlayArea
→ store bDestinationCommitted + frozen destination spec
→ DO NOT teleport and DO NOT complete Controller from visual callback
→ finish EnteringPlayArea normally
→ immediately enter DestinationTail
```

If arrival finishes first:

```text
EnteringPlayArea finishes
→ AtPlayArea
→ wait visually until exact destination Record formally commits
→ DestinationTail
```

### 7.6 Formal ownership always wins

A detached job never owns Gameplay card-zone state.

If the same RuntimeId later becomes a formal Hand/zone owner while an old tail exists:

```text
formal owner wins immediately
→ retire/cancel the old exact visual job
→ never hide/remove/move the new formal Widget
```

Old callbacks are exact-token no-ops after retirement.

### 7.7 GC and finite lifetime

Use a GC-reachable deterministic owner, conceptually:

```cpp
UPROPERTY(Transient)
TArray<FDetachedCardVisualJob> DetachedCardVisualJobs;
```

Any UObject/Widget held by a live job must be strongly reachable through `UPROPERTY`/`TObjectPtr` or an
equivalent GC-safe owner.

Each visual phase must have finite positive duration. NativeTick may advance private cosmetic phases, but
NativeTick does not decide Gameplay readiness.

Mandatory cleanup boundaries include:

```text
HUD deactivation/destruction
session invalidation
Controller/battle replacement
terminal/unavailable
runtime detached-card disable
Global Skip/backlog collapse
recovery for the affected chronology
same-RuntimeId formal-owner collision
unsafe viewport/host geometry change
```

Cleanup is exact and idempotent.

A defensive high ceiling may decline new visual preparation before formal commit; no random eviction or
container-order eviction is allowed.

## 8. Canonical card historical semantics

G9 must not add a third CardPlayed/CardZone validator in a detached path.

Before NonBlocking activation, extract or centralize one Controller-side card historical reducer contract,
conceptually:

```cpp
bool TryApplyCardPlayedRecord(
    FPresentationStateSnapshot& Snapshot,
    const FCardPlayedPresentationPayload& Payload);

bool TryApplyCardZoneChangedRecord(
    FPresentationStateSnapshot& Snapshot,
    const FCardZoneChangedPresentationPayload& Payload,
    const FPlayedCardPresentationLifecycleToken* PlayedLifecycleContext);
```

Exact API shape may differ, but semantics are unique.

### 8.1 CardPlayed historical contract

It must preserve the sealed historical requirements currently split between Controller and Native Widget,
including as applicable:

```text
valid frozen card identity
unique exact RuntimeId/CardId in historical Hand
exact HandIndexBefore
exact historical EnergyBefore
valid EnergyAfter / CostPaid relationship under the current producer contract
valid source/optional target Presentation identity under the current producer contract
valid PlayArea lifecycle shape
```

The reducer mutates only the candidate formal snapshot. Widget/host geometry is not part of historical
validity.

### 8.2 PlayArea destination needs lifecycle context

The frozen snapshot does not itself contain an authoritative PlayArea card array. Therefore a
`PlayArea -> destination` Record cannot prove the source lifecycle from snapshot counters alone.

Its exact historical contract additionally requires the Controller-owned unresolved
`FPlayedCardPresentationLifecycleToken` for the same RuntimeId/CardId and expected chronology.

Missing/mismatched lifecycle correlation is a historical Presentation failure and follows active-envelope
recovery. It is not a cosmetic eligibility decline.

### 8.3 Visual eligibility is separate

Visual eligibility includes only things such as:

```text
DetachedCardVFXHost exists
CardWidgetClass/resource exists
required formal source/anchor geometry is valid
host geometry is finite/non-zero
AbsoluteToLocal conversion succeeds
per-job phase durations are finite positive
visual-job capacity is available
```

Thus:

```text
historical invalid / lifecycle correlation invalid
→ recovery

historical valid + visual prepare ineligible BEFORE formal commit
→ sealed Blocking path when such fallback is still valid

formal commit already happened + visual activation/update failure
→ drop private visual only
→ never replay reducer
→ never roll back formal card-zone state
```

### 8.4 Blocking migration uses the same semantics first

G9-C must migrate the existing Blocking R8 card path onto the canonical reducer/lifecycle correlation before
G9-D changes timing.

Blocking migration may validate a candidate copy before visual start and formally commit once on exact
Blocking completion. The important invariant is one semantic rule and one formal state transition, not one
particular helper call count.

## 9. Relationship with sealed FastInput

The two mechanisms have different meanings:

```text
FastInput
→ new physical click chooses to collapse real Blocking chronology
→ SkipPresentation
→ exact catch-up retry

G9 buffered selection
→ new physical click occurs in an exact G9 card-visual window
→ do NOT Skip
→ wait for exact sealed target surface
→ replay card selection once
```

Decision ordering for each NEW physical card click:

```text
1. exact normal selection surface ready now?
   → clear old G9 buffer
   → normal SelectCard

2. exact G9 bufferable card anchor + Controller target credential?
   → capture/replace G9 intent
   → no Skip

3. otherwise sealed FastInput catch-up eligible?
   → clear old G9 intent
   → preserve G8 FastInput behavior

4. otherwise
   → clear/retain only according to explicit current request policy
   → normal reject/base behavior
```

Once a G9 intent has been captured, intermediate Damage/Status/other chronology does **not** automatically
convert that old click into FastInput. A future Skip requires a new physical input path.

A pure detached card tail remains:

```text
HasSkippablePresentationDelay() == false
```

It can be a G9 buffer anchor, but it is never a reason to call `SkipPresentation()`.

Ordinary Global Skip caused by another legitimate route clears current buffered G9 intent and cancels
current-session detached card visuals according to G9 cleanup policy.

## 10. Card-record migration and NonBlocking transaction strategy

### 10.1 G9-C: migrate ownership while still Blocking

Before any card Record becomes NonBlocking:

```text
single NativePlayedCardWidget + singleton animation state
→ Controller played-card lifecycle token
→ exact per-job visual owner
→ DetachedCardVFXHost
→ per-job animation state
```

Controller timing remains equivalent to R8:

```text
start exact Blocking visual job
→ Controller waits on existing exact playback contract
→ exact finish
→ canonical reducer formal commit
→ publish
→ next Record
```

The temporary Blocking adapter may receive private job completion to satisfy the existing tracked playback
token. This completion edge is removed for jobs once that phase becomes detached in G9-D.

### 10.2 G9-D1: detach PlayArea -> destination tail first

`CardPlayed` remains Blocking and reaches its normal PlayArea visual point.

For eligible PlayArea destination:

```text
validate canonical historical record + exact unresolved played lifecycle
prepare destination visual spec/job continuation
pre-commit exact recheck
commit destination reducer exactly once
publish formal snapshot
mark played lifecycle destination formally consumed
post-publication exact recheck
activate/continue private DestinationTail
advance Controller immediately
```

After formal commit:

```text
DestinationTail finish
→ cleanup exact job only
→ no Controller completion
→ no readiness mutation
```

This stage already produces the key overlap:

```text
A destination tail alive
+ exact next normal surface ready
→ B can be selected/confirmed/played through normal fresh inputs
→ A tail continues privately
```

### 10.3 G9-D2: detach CardPlayed arrival only with end-to-end sealed correlation

D2 is higher risk because Controller may reach the destination Record while the card is still visually
entering PlayArea.

Therefore `CardPlayed` may detach only if the Controller can prove, from already-sealed chronology, an exact
supported future played-card destination lifecycle.

Before formal CardPlayed commit, D2 must preflight at least:

```text
exact current CardPlayed Record identity
canonical CardPlayed candidate snapshot
exact current Session/Battle/record cursor
one supported future matching PlayArea -> {Discard, Exhaust, Removed} Record in the sealed chronology
no ambiguous second matching unresolved lifecycle
required card visual host/resources
frozen Hand start geometry
frozen PlayArea geometry
future destination visual endpoint/spec can be safely prepared or represented
finite phase timings / capacity
```

This is not early Gameplay execution. The future destination Record is already a committed/sealed
Presentation fact; G9 may freeze its cosmetic endpoint, but must not apply its reducer or start its
destination phase before chronology reaches that Record.

If this end-to-end preflight fails:

```text
CardPlayed detach DECLINES BEFORE formal commit
→ sealed Blocking CardPlayed path
→ D1 may still detach the later destination when it is reached
```

Detached CardPlayed transaction:

```text
prepare hidden EnteringPlayArea job
canonical CardPlayed candidate reducer
pre-commit exact recheck
formal CardPlayed commit exactly once
create exact unresolved PlayedCardPresentationLifecycleToken
publish formal snapshot
post-publication exact recheck
activate EnteringPlayArea job
advance Controller immediately
```

When the future exact destination Record is reached:

```text
validate canonical destination reducer + exact lifecycle token
commit destination formal snapshot exactly once
publish
mark lifecycle destination consumed
if exact visual job still exists:
    record bDestinationCommitted + frozen destination spec
    EnteringPlayArea -> finish arrival -> DestinationTail
    OR AtPlayArea -> DestinationTail immediately
if visual job is already gone:
    formal chronology still advances normally
advance Controller immediately
```

The destination Record never waits for the detached arrival to finish.

### 10.4 Failure boundary after detached CardPlayed formal commit

Once detached CardPlayed has formally committed, the later destination path cannot assume sealed R8 still has
a retained `NativePlayedCardWidget` in formal `OV_PlayArea`.

Therefore after that commit:

```text
visual loss / feature disable / geometry loss
→ may remove private card job
→ MUST NOT remove the Controller played-card lifecycle correlation prematurely
→ later exact destination Record still commits formal state from canonical reducer + lifecycle token
→ no attempt to resurrect/replay CardPlayed
```

This is why lifecycle correlation is Controller-owned and visual-job-independent.

### 10.5 Transaction reentrancy rules

As in G8, ViewModel publication may synchronously trigger replacement, disablement, recovery or other callbacks.
Every detached commit path requires:

```text
pre-commit exact Session/Battle/record/lifecycle recheck
formal commit exactly once
publication
post-publication exact recheck
only then activate/update private visual state
```

A post-commit stale result is `Consumed`; it never falls back and never replays the reducer.

## 11. Proposed stages

### G9-A — Buffered Target / Intent Foundation

Goal: build exact non-predictive target identity and stale fencing without production UX change.

Implement/shadow:

```text
Controller-authoritative TryCaptureBufferedCardTarget or equivalent
FBufferedCardTargetCredential
FBufferedCardWindowIdentity
single FBufferedCardIntent owner
wait-vs-stale evaluator
atomic consume helper
mandatory clear boundaries
second-click credential replacement rules
Automation-only shadow capture/revalidation
```

Do not yet:

```text
change Hand hover
change FastInput Skip behavior
replay buffered selection in production
change card visual ownership
```

Stop gate: A must prove no predicted revision and no Gameplay request from the shadow path.

### G9-B — Buffered Hand Selection + Hover

Goal: first player-visible improvement while card presentation remains Blocking.

Implement:

```text
dirty-aware/stable formal Hand reconciliation
split structural layout from hover affordance
allow surviving formal Hand cards to hover in exact G9 Blocking-card window
click B captures buffered selection instead of Skip
replay B selection once at exact sealed normal surface
```

Expected UX:

```text
A Blocking card animation active
→ B hover works
→ early click B does not Skip A
→ no B Gameplay occurs early
→ at exact ready surface B becomes selected once
→ fresh Confirm/Target input is still required
```

### G9-C — Canonical Card Reducer + Multi-instance Visual Ownership, still Blocking

Goal: remove singleton ownership before timing changes.

Implement:

```text
canonical CardPlayed/CardZone historical reducer contract
Controller FPlayedCardPresentationLifecycleToken or equivalent
exact unresolved lifecycle correlation
DetachedCardVFXHost
FDetachedCardVisualToken
GC-safe deterministic job container
per-job phase/elapsed/geometry/opacity/scale state
Blocking completion adapter
cleanup/replacement/recovery behavior
same-RuntimeId formal-owner protection
```

Production timing remains Blocking.

Stop gate: C must prove R8 timing/behavior parity independently of NonBlocking activation.

### G9-D1 — NonBlocking destination tail

Goal: detach only:

```text
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

while keeping CardPlayed Blocking.

Required result:

```text
formal destination commits immediately
A DestinationTail continues privately
exact next player surface may become ready while A tail is alive
pure A tail does not create HasSkippablePresentationDelay
```

### G9-D2 — NonBlocking CardPlayed arrival

Goal: detach CardPlayed only after D1 is proven.

Required result:

```text
CardPlayed formal commit can advance before visual arrival completes
future exact destination Record may commit before arrival completes
job records pending destination command and transitions correctly
Controller chronology never waits for the detached job
```

D2 must keep the full end-to-end preflight/fallback boundary from §10.3.

### G9-E — Integration / Cleanup

No new feature is added here. Validate/clean:

```text
FastInput coexistence
buffer anchor during intermediate Damage/Status chronology
PendingSelection
ReadyToConfirm
TargetChoice
runtime disable
Global Skip
active-envelope recovery
HUD replacement
Controller replacement
battle replacement
terminal/unavailable
DirectBaseline exclusion
same-RuntimeId redraw/replay
GC / destruction
viewport/layout changes
G8 DamageNumber coexistence
G6 Selection Group regression
```

### G9-F — Evidence / Seal

Only after build + affected Automation + manual PIE + evidence review may G9 become:

```text
G9 — COMPLETE / VALIDATED / SEALED
```

## 12. Automation requirements

At minimum dedicated G9 tests must prove the following.

### 12.1 Buffered target authority

```text
capture succeeds only with PresentationOwned exact Session
capture target comes from an already-sealed exact frozen baseline
exact player-facing read must match target BattleId + revision
pending-selection / terminal / unavailable target cannot be captured
DirectBaseline does not fabricate a target SessionToken
requested card absent from target Hand declines capture
no CurrentRevision+1 / numeric prediction path exists
```

### 12.2 Buffered intent lifecycle

```text
A eligible card window -> B intent captured -> no Skip
while displayed revision is older but sealed target is unchanged -> intent waits
sealed target revision changes -> old intent drops
exact ready -> B SelectCard replay exactly once
reentrant ViewModel broadcast cannot replay twice
stale SessionToken -> drop
stale BattleId -> drop
HUD/Controller/battle replacement -> drop
B leaves Hand -> drop
B becomes unplayable -> drop
PendingSelection appears -> drop
terminal/unavailable -> drop
runtime disable -> drop unconditionally
```

### 12.3 Interaction transition boundary

```text
buffered no-target card
→ replay selects card
→ ReadyToConfirm
→ no automatic Confirm / no Gameplay until fresh Confirm

buffered target card
→ replay selects card
→ ChoosingTarget
→ no automatic target / no Gameplay until fresh target click
```

### 12.4 Repeated click / FastInput coexistence

```text
second click same exact TargetCredential + CaptureWindow -> RuntimeId may replace
second click different valid credential -> entire intent credential replaced
new FastInput physical click -> old G9 intent cleared before Skip
captured G9 intent is not auto-converted to FastInput by later intermediate Records
pure detached card tail -> HasSkippablePresentationDelay false
```

### 12.5 Hand hover / structural stability

```text
non-Hand publication does not needlessly recreate formal Hand Widgets
hover-only update does not call structural layout
hidden A historical source is not hoverable
surviving B hover works during exact eligible window
real Hand change reconciles authoritatively and safely resets hover if needed
```

### 12.6 Canonical card reducer / lifecycle correlation

```text
Blocking and detached paths share one CardPlayed historical rule
Blocking and detached paths share one CardZone historical rule
PlayArea destination requires exact unresolved lifecycle token
missing/wrong lifecycle token -> recovery, not cosmetic fallback
formal destination consumes lifecycle exactly once
same RuntimeId later play receives a new lifecycle generation
```

### 12.7 Card visual ownership

```text
A and B visual jobs coexist
all animation state is per-job
A callback cannot mutate B
completion order inversion is harmless
wrong/stale token completion is no-op
exact cleanup removes only exact job
formal OV_PlayArea child invariants are unaffected by detached tails
DetachedCardVFXHost is non-interactive
same RuntimeId redraw retires old tail without touching new formal Widget
old tail cannot restore stale opacity/visibility/transform
```

### 12.8 D1 destination detach transaction

```text
pre-commit visual decline -> Blocking fallback, zero formal side effect
destination formal reducer commits once
publication occurs before private detached activation
post-commit visual failure -> visual dropped only
destination-tail finish has no Controller/readiness callback
```

### 12.9 D2 arrival/destination phase handoff

```text
D2 refuses detach when no exact sealed future supported destination exists
D2 refuses ambiguous future lifecycle correlation
CardPlayed formal commit creates exact unresolved lifecycle
Destination Record may commit while job still EnteringPlayArea
job stores exact pending destination command
arrival completion then begins DestinationTail
arrival may finish first and wait AtPlayArea until destination commit
visual loss before destination does not lose formal lifecycle correlation
destination formal state still commits with no visual job
```

### 12.10 Recovery / lifecycle

These boundaries:

```text
Global Skip
active-envelope reconcile
HUD replacement
Controller replacement
battle replacement
terminal
unavailable
runtime feature disable
Widget destruction/deactivation
viewport geometry invalidation
GC
```

must leave:

```text
no ghost card
no duplicate card
no stale callback mutation
no stuck input
no lost formal Hand card
no stranded unresolved lifecycle token
no buffered intent replay after its authority was invalidated
```

## 13. Required regressions

G9 validates affected sealed contracts rather than arbitrary whole-project reruns. Expected minimum regression
set includes the current equivalents of:

```text
G8-D detached Damage behavior
G8-B exact readiness / session fencing
Phase6UIA2N.FastInput
R8 Native Card Lifecycle
CardSelection.Unified
G5/G6 Selection Presentation lifecycle/group behavior
TargetChoice / ReadyToConfirm interaction tests
```

Exact suite names/counts are recorded from the current tree in the eventual G9 execution/evidence document,
not frozen prematurely here.

## 14. Manual PIE gates

### 14.1 G9-B PIE — Blocking card + buffered selection

```text
play A
→ while A Blocking card animation is active, hover surviving B
→ B raises/scales normally
→ click B
→ A does not Skip
→ B Gameplay does not execute early
→ at exact target surface B becomes selected once
```

Then verify both modes:

```text
no-target B
→ ReadyToConfirm
→ requires fresh Confirm

target B
→ ChoosingTarget
→ requires fresh target click
```

### 14.2 G9-C PIE — visual parity before detach

```text
CardPlayed / destination movement visually matches R8 expectations
formal OV_PlayArea has no detached-tail contamination
Skip/cancel/recovery leaves no retained card
same RuntimeId formal return cannot be damaged by old job
```

Timing remains Blocking in this stage.

### 14.3 G9-D1 PIE — destination tail overlap

```text
play A
→ CardPlayed arrival completes normally
→ destination Record formally commits
→ A destination tail continues
→ exact player surface becomes interactive while A tail is alive
→ select/Confirm/Target/play B normally
→ A tail is not Skipped merely because B input is accepted
→ A/B visual lifetimes may overlap
```

### 14.4 G9-D2 PIE — detached arrival + pending destination

Use at least one card whose committed resolution contains intervening records:

```text
play A
→ A CardPlayed arrival is detached and visibly still moving
→ committed Damage/Status/other Records may continue chronologically
→ hover/click surviving B can buffer without Skip while exact A visual anchor is alive
→ destination Record may formally commit before A visually reaches PlayArea
→ A does not teleport or flash back
→ A completes arrival then destination tail
→ B selection replays only at exact normal surface
→ fresh Confirm/Target is still required
```

Observe throughout:

```text
no duplicate
no ghost card
no flashback
no stale opacity/visibility restore
no formal zone rollback
no stuck input
no accidental Skip caused only by detached card tail
DamageNumber and card-tail cosmetics coexist without owning Gameplay readiness
```

### 14.5 Same-RuntimeId redraw/replay scenario

When achievable:

```text
old A destination tail alive
→ same RuntimeId formally returns to Hand
→ formal Hand card is visible/correct/interactive
→ old tail is retired safely
→ same RuntimeId can later begin a NEW played-card lifecycle
→ old callback cannot mutate the new lifecycle/widget
```

## 15. Feature-disable, Skip and recovery contracts

### 15.1 Runtime G9 disable without authority replacement

Runtime disable is a feature-policy change, not a Presentation authority replacement:

```text
keep current PresentationSessionToken
clear buffered intent UNCONDITIONALLY
cancel all current private detached-card visual jobs
stop accepting new G9 buffered captures
stop accepting new detached-card transactions
future new eligible card lifecycles use sealed Blocking path
re-evaluate input through existing exact guards
```

Do not mint a new SessionToken merely because G9 is disabled.

However:

```text
already-formally-committed detached CardPlayed
+
unresolved played-card lifecycle awaiting its exact destination
```

must keep the **Controller lifecycle correlation** until that destination chronology is formally consumed or
collapsed by Skip/recovery. Feature disable may delete the cosmetic job, but cannot delete the correlation
needed to validate already-committed history.

### 15.2 Global Skip / backlog collapse

A legitimate sealed Skip path:

```text
clears current G9 buffered intent
cancels current-session detached card visuals
clears/retire played-card lifecycle correlations whose chronology is collapsed
preserves or replaces SessionToken exactly according to the sealed G8 Skip/replacement contract
```

A pure detached card tail alone never makes Skip eligible.

### 15.3 Active-envelope recovery/reconcile

Ordinary G8 recovery may preserve the PresentationSessionToken, but G9 must still clear buffered intents and
card lifecycle/job state associated with the recovered/collapsed chronology because their exact target/Record
proof is no longer valid.

```text
same SessionToken
!=
G9 intent/lifecycle still valid after chronology recovery
```

If recovery upgrades to authority replacement/unavailable, normal Session invalidation rules also apply.

### 15.4 PendingSelection / TargetChoice

A new authoritative PendingSelection boundary clears buffered card intent. Detached private card tails may
continue only if they do not collide with formal ownership; they never grant PendingSelection readiness.

A replayed target card entering `ChoosingTarget` consumes the buffer. The detached old card tail may continue,
but the next target click is a new input.

## 16. Non-goals

G9 v1 does not authorize:

```text
parallel Gameplay resolutions
multiple-command buffered queue
speculative energy reservation
speculative target reservation
buffered Confirm/Cancel/EndTurn
auto-confirm or auto-target from an old card click
numeric future-revision prediction
generalized detached framework for every Presentation Record
detached DrawPile -> Hand
detached Selection Group
rewriting G8 DamageNumber ownership
changing Gameplay card-zone authority
using RuntimeId as visual-job occurrence identity
letting detached visuals occupy formal OV_PlayArea/Hand ownership
```

Future broader Presentation overlap requires separate evidence after G9 is sealed.

## 17. Delivery order and stop gates

Recommended execution order:

```text
G9-A  target credential / buffered intent shadow authority
→ build + focused Automation

G9-B  dirty-aware Hand hover + buffered selection production activation
→ build + focused Automation + manual PIE

G9-C  canonical card reducer + lifecycle correlation + multi-job host migration, still Blocking
→ build + R8-focused Automation + manual parity PIE

G9-D1 destination-tail NonBlocking activation
→ build + D1/G8/FastInput/card regressions + manual overlap PIE

G9-D2 CardPlayed-arrival NonBlocking activation + pending-destination phase handoff
→ build + D2/D1/G8/card regressions + manual overlap PIE

G9-E  integration cleanup
→ affected integration matrix

G9-F  evidence / seal
```

Hard stop gates:

```text
A -> B
requires exact non-predictive target capture + stale fencing proven

B -> C
requires buffered selection UX proven without changing card chronology

C -> D1
requires canonical reducer/lifecycle correlation + job ownership proven while still Blocking

D1 -> D2
requires one-way destination-tail completion and host isolation proven

D2 -> E
requires destination-before-arrival handoff, feature-disable/recovery and same-RuntimeId ABA coverage proven
```

Do not mark G9 complete based only on buffered selection. Full v1 completion requires the reviewed detached
card lifecycle scope and final integration evidence.

## 18. Current status and frozen design candidate

At this review revision:

```text
G8    — COMPLETE / VALIDATED / SEALED
G9    — DESIGN REVIEWED / NOT IMPLEMENTED / NOT SEALED
G9-A  — NOT STARTED
G9-B  — NOT STARTED
G9-C  — NOT STARTED
G9-D1 — NOT STARTED
G9-D2 — NOT STARTED
G9-E  — NOT STARTED
G9-F  — NOT STARTED
```

Frozen candidate principles:

```text
Gameplay remains serial and authoritative
buffered click is selection-only, never auto-confirm/auto-target
ExpectedReadyRevision is exact sealed authority, never prediction
Controller mints target credential; UI owner holds one pending physical intent
replay is readiness-event driven and atomically consumed
new physical input never inherits an old credential accidentally
Hand structural reconciliation and hover affordance are separate
non-Hand updates do not needlessly rebuild formal Hand
played-card lifecycle occurrence has exact Controller identity independent of RuntimeId
visual job identity is independent of both Gameplay card identity and formal lifecycle identity
DetachedCardVFXHost is physically separate from formal OV_PlayArea/Hand
all overlapping card animation state is per-job
canonical card reducer semantics precede NonBlocking activation
PlayArea destination requires exact unresolved played lifecycle correlation
D1 detaches destination tail before D2 detaches CardPlayed arrival
D2 requires end-to-end sealed destination correlation before CardPlayed formal commit
future destination may formally commit before detached arrival finishes
private visual finish never completes Controller chronology or grants readiness
formal ownership always wins same-RuntimeId collisions
pure detached card tail never becomes HasSkippablePresentationDelay
runtime feature disable clears buffered intent but preserves already-required formal lifecycle correlation
G8 sealed behavior remains the authority outside explicitly activated G9 scope
```

No production C++ change is authorized by this document alone.