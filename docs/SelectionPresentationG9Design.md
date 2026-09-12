# Selection Presentation G9 — Buffered Player Input + Detached Card Presentation

Date: **2026-09-13**

Status: **DESIGN REVIEWED / NOT IMPLEMENTED / NOT SEALED**

Authority baseline: [`SelectionPresentationG8FSeal.md`](SelectionPresentationG8FSeal.md).

G8 is sealed. G9 is a new initiative and must not reopen sealed G8 behavior for speculative cleanup.
This document defines the reviewed scope, sequencing, ownership rules, stale fencing, migration strategy,
and acceptance criteria for two player-facing improvements:

```text
1. buffered player input
   - buffered card selection
   - buffered EndTurn intent

2. detached played-card visual presentation
```

This document does **not** authorize production implementation by itself.

The reviewed G9 architecture freezes these principles before implementation begins:

```text
1. Gameplay remains serial and authoritative even when Presentation visuals overlap.
2. Presentation animation alone must not own EndTurn availability.
3. During the player's turn, EndTurn remains available through ordinary animation and ordinary resolving;
   if it cannot execute safely yet, the EndTurn intent is buffered.
4. Authoritative mandatory selection is the primary player-turn condition that disables EndTurn.
5. Buffered EndTurn has priority over an older buffered card selection.
6. A buffered EndTurn never calls SkipPresentation merely to become executable.
7. A buffered card click replays card selection only; it never replays Confirm or Target input.
8. ExpectedReadyRevision for buffered card selection is Controller-authoritative and already sealed; it is never predicted.
9. Buffered-input ownership and replay are readiness/event driven, never cosmetic-tick driven.
10. Card Presentation gets a Controller-owned played-card lifecycle identity separate from RuntimeId and visual-job identity.
11. Detached card jobs own all per-instance phase/geometry/elapsed/opacity/scale state.
12. Detached card visuals live in a dedicated non-interactive host and never occupy formal Hand / OV_PlayArea ownership.
13. Card historical reducer semantics are centralized before NonBlocking activation.
14. G9-D is split into D1 destination-tail detach first and D2 CardPlayed-arrival detach second.
15. D2 is allowed only when the future supported PlayArea destination is already sealed and exactly correlatable.
16. Feature disable clears buffered player input, while already-required formal card lifecycle correlation survives long enough to finish chronology safely.
17. Hand hover is separated from structural layout; unrelated ViewModel publications must not needlessly rebuild the formal Hand.
```

---

## 1. Goal

G9 targets a Slay-the-Spire-like input feel without sacrificing deterministic Gameplay chronology.

### 1.1 Card-selection experience

```text
play card A
→ A Presentation continues

while A visual is still alive:
→ surviving formal Hand card B can hover / raise normally
→ clicking B does not Skip A merely because A is still visible

if the exact normal player-card selection surface is not displayed yet:
→ capture one buffered card-selection intent for B
→ do not execute B Gameplay early

when the exact sealed player-card surface is displayed:
→ revalidate Battle / Session / revision / RuntimeId / legality
→ replay SelectCard(B) exactly once if still valid
```

The buffered click is selection only:

```text
buffered B has no target
→ replay enters ReadyToConfirm
→ NEW physical Confirm is required

buffered B requires a target
→ replay enters ChoosingTarget
→ NEW physical Target click is required
```

One old click must never cross multiple interaction-state transitions.

### 1.2 EndTurn experience

During an authoritative player turn, ordinary Presentation or ordinary resolving must not make the EndTurn
control unavailable merely because visuals/chronology are still catching up.

Desired behavior:

```text
DamageNumber is still alive
→ EndTurn can be clicked

card A is still flying to PlayArea / Discard / Exhaust
→ EndTurn can be clicked

Draw / Shuffle / Status / other ordinary Presentation is still playing
→ EndTurn can be clicked

ordinary Gameplay resolution is still finishing
→ EndTurn can be clicked
→ request is buffered if it cannot safely execute yet
```

If EndTurn is already safe and legal, execute it normally. If not:

```text
physical EndTurn click
→ capture one BufferedEndTurnIntent
→ do not Skip Presentation
→ wait for the next exact legal EndTurn surface
→ revalidate authority
→ execute EndTurn exactly once
```

The main exception is authoritative mandatory selection:

```text
"choose one card to exhaust"
"choose N cards to discard"
other PendingCardSelection / mandatory card-choice contract
→ EndTurn is disabled/rejected until that mandatory decision is completed
```

`ReadyToConfirm` and `ChoosingTarget` are **not** mandatory selections. EndTurn remains available there:

```text
EndTurn from ReadyToConfirm
→ cancel the transient selected-card state
→ EndTurn immediately or buffer it until safe

EndTurn from ChoosingTarget
→ cancel the transient target/card selection
→ EndTurn immediately or buffer it until safe
```

### 1.3 Core invariant

> **G9 may overlap visual lifetimes and may buffer player intent, but it must never overlap authoritative Gameplay resolutions.**

The authoritative flow remains serial:

```text
physical player intent
→ exact legal request boundary
→ Gameplay / BattleActionQueue / reducer
→ committed Presentation facts
→ next exact legal request boundary
```

Visual overlap and buffered input never authorize speculative Gameplay.

---

## 2. Blocking semantics after G9

G9 separates three concepts that must no longer be conflated:

```text
A. Controller/Presentation Blocking
B. card-selection availability
C. EndTurn availability
```

A Presentation record may remain Blocking for Controller chronology and may still block ordinary card
selection, while **not** disabling the EndTurn button.

Example:

```text
DrawPile -> Hand remains Blocking in G9 v1

meaning:
→ Controller still waits for the draw Presentation contract
→ ordinary next-card interaction may remain unavailable

but NOT meaning:
→ EndTurn button must be disabled solely because the draw animation is playing
```

EndTurn has its own authority contract in §7.

---

## 3. Current baseline and why G9 needs separate architecture

### 3.1 G8 FastInput is catch-up by Skip

The sealed FastInput card path captures an exact catch-up credential and, when real Blocking chronology owns
the delay, may perform:

```text
card click
→ capture exact catch-up target
→ SkipPresentation()
→ exact retry
```

That remains valid for its existing purpose. G9 does not reinterpret every card click as buffered input.

### 3.2 Hand interaction currently mixes layout and hover

Current Native Hand interaction couples layout work and hover transforms. G9-B must separate:

```text
formal Hand structural reconciliation
from
visual-only hover affordance
```

Unrelated HP/Status/Energy publications must not needlessly destroy/recreate Hand Widgets.

### 3.3 Card Presentation currently has singleton ownership

The sealed R8 lifecycle retains one played-card Widget across `CardPlayed -> PlayArea destination`, while the
Native animation path also stores singleton moving-card animation state.

True overlap requires per-lifecycle and per-job ownership; converting one pointer into an array is insufficient.

### 3.4 Detached visuals cannot live in formal PlayArea

Formal `OV_PlayArea` has historical shape/child-count semantics. Old detached tails inside that container
would contaminate later `CardPlayed` validation.

G9 therefore requires a dedicated non-interactive detached-card visual host.

### 3.5 PresentationOwned versus DirectBaseline remains sealed

```text
PresentationOwned
→ Controller + exact PresentationSessionToken

DirectBaseline
→ intentionally sessionless direct delivery
```

Buffered card Presentation-lag behavior is PresentationOwned-only.

EndTurn in DirectBaseline follows the normal direct legal EndTurn path. G9 does not fabricate a fake
PresentationSessionToken for DirectBaseline.

---

## 4. G9 v1 scope

### 4.1 Detached card Presentation scope

Reviewed candidates:

```text
CardPlayed
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Activation is staged:

```text
G9-D1
→ detach PlayArea -> destination tails
→ CardPlayed arrival remains Blocking

G9-D2
→ detach CardPlayed arrival
→ destination may formally commit before visual arrival finishes
```

### 4.2 Presentation paths that remain Blocking in v1

Unless amended later:

```text
DrawPile -> Hand
Hand -> Discard at turn cleanup
Hand -> Exhaust outside the played-card lifecycle
SelectionArea transitions
G6 multi-selection Group
Shuffle
PendingSelection presentation
TargetChoice presentation
formal Energy / HP / Block / Status chronology
Terminal / unavailable / recovery
```

Again: **Blocking here does not automatically mean EndTurn-disabled.**

### 4.3 Buffered player-input scope

G9 v1 supports two mutually exclusive buffered player intents:

```text
BufferedCardSelection
BufferedEndTurn
```

It does not buffer:

```text
Confirm
Cancel
Target click
PendingSelection submit/cancel
multiple future card commands
multiple EndTurn commands
```

There is at most one pending G9 player intent at a time.

Conceptually:

```cpp
enum class EBufferedPlayerIntentKind : uint8
{
    None,
    CardSelection,
    EndTurn
};
```

Exact implementation shape is not frozen.

---

## 5. Buffered input ownership and arbitration

### 5.1 One owner

One battle-HUD input owner owns at most one pending G9 player intent.

Do not duplicate pending intent state independently across HUD, ViewModel and Controller.

Responsibilities:

```text
Controller
→ mints exact Presentation authority credentials

ViewModel/Battle
→ owns current interaction legality / Gameplay legality

G9 input owner
→ owns the one pending physical player intent
```

### 5.2 EndTurn wins over an older card intent

When a physical EndTurn click is accepted:

```text
clear BufferedCardSelection first
cancel/retire any pending G9 card replay
cancel transient ReadyToConfirm / ChoosingTarget selection if present
then execute EndTurn now or store BufferedEndTurn
```

An old card click must never fire after the player has explicitly requested EndTurn.

If a sealed G8 FastInput card retry has already been scheduled, G9 integration must ensure an accepted
EndTurn cannot be followed by that stale retry. This may require a narrow cancellation/retirement hook, but
must not change G8 FastInput catch-up semantics outside the explicit G9 arbitration path.

### 5.3 Buffered EndTurn is decisive

Once a `BufferedEndTurn` has been accepted for the current player-turn authority window:

```text
new ordinary card selections are not accepted/buffered
until EndTurn executes or the EndTurn intent is invalidated
```

This mirrors a normal EndTurn click: once the command is accepted, it is the player's terminal command for
that current turn-input window.

### 5.4 Replay/evaluation is event driven

Buffered input evaluation may be triggered by authoritative Presentation/ViewModel/readiness transitions.

It must not be driven by:

```text
NativeTick polling for Gameplay readiness
DamageNumber completion
DetachedCardVisualJob completion
arbitrary cosmetic timer expiry
```

Cosmetic NativeTick may animate private jobs only.

---

## 6. Buffered card-selection authority

### 6.1 Identity

Conceptual state:

```cpp
struct FBufferedCardTargetCredential
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    int64 ExpectedReadyRevision = 0;
};

struct FBufferedCardWindowIdentity
{
    int64 SourceResolutionId = 0;
    int64 SourcePresentationSequence = 0;
    uint64 LocalWindowGeneration = 0;
};

struct FBufferedCardIntent
{
    FBufferedCardTargetCredential Target;
    FBufferedCardWindowIdentity CaptureWindow;
    int32 RuntimeId = INDEX_NONE;
};
```

RuntimeId is Gameplay card identity, not visual-job identity.

### 6.2 ExpectedReadyRevision is exact and already sealed

A Controller-authoritative helper such as:

```cpp
bool TryCaptureBufferedCardTarget(
    int32 RequestedRuntimeId,
    FBufferedCardTargetCredential& OutCredential) const;
```

may succeed only when the intended target player surface is already an exact frozen authoritative fact.

It must not invent:

```text
CurrentRevision + 1
numeric future revision guesses
"eventually this should be player ready"
```

Required authority includes, conceptually:

```text
PresentationOwned current
exact SessionToken
same BattleId
latest frozen baseline exists
baseline identifies the intended player-turn target
Outcome == None
no authoritative mandatory PendingCardSelection owns that target
requested RuntimeId exists as the same authoritative card instance
no unavailable / terminal / recovery ambiguity
```

### 6.3 Waiting and replay

While display is behind the exact target:

```text
same Session/Battle
+
latest sealed target is still the exact ExpectedReadyRevision
→ wait

sealed target changes
→ stale; drop
```

Do not use `<`, `>`, or `>=` as authority ranges.

Replay requires all exact conditions again, including:

```text
same SessionToken
same BattleId
latest frozen target still exact
current displayed revision == ExpectedReadyRevision
exact player-facing read matches
Outcome == None
normal card-selection surface
no mandatory PendingSelection
RuntimeId still in current authoritative Hand
same card instance
normal live QueryCardPlayability passes
```

Then:

```text
Take buffered card intent
clear stored intent FIRST
call normal SelectCardByRuntimeId(RuntimeId) once
```

Never automatically restore a failed/rejected old click.

### 6.4 Repeated card clicks

Each physical click captures authority again.

```text
same exact target credential + same capture window
→ RuntimeId-only replacement is permitted

different valid credential
→ replace the whole buffered-card intent
```

Never attach a newly clicked card to an older credential accidentally.

### 6.5 Card-intent stale boundaries

Clear/drop on:

```text
Session replacement
HUD / Controller / Battle replacement
BattleId change
authority transition / DirectBaseline
presentation unavailable
terminal
mandatory PendingSelection boundary
Global Skip / backlog collapse
recovery involving target chronology
runtime G9 disable
sealed target revision replacement
accepted EndTurn
card leaving Hand / becoming illegal at replay
```

---

## 7. Buffered EndTurn authority

EndTurn intentionally uses a different contract from exact-target card selection.

A card selection is tied to one exact future player-card surface. EndTurn represents:

> **"End this same authoritative player turn at the next legal EndTurn boundary."**

It therefore must not guess one exact future revision just to remain pending.

### 7.1 Conceptual identity

```cpp
struct FBufferedEndTurnIntent
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    int64 CaptureStateRevision = 0; // provenance/diagnostic, not a replay target
    uint64 LocalIntentGeneration = 0;
};
```

Exact shape may differ.

`CaptureStateRevision` must not be interpreted as `ExpectedReadyRevision` and must not authorize numeric
range replay.

### 7.2 EndTurn button availability contract

During the player's turn, the EndTurn control should remain available unless an authoritative rule requires
it to be unavailable.

EndTurn is enabled/accepting through:

```text
ordinary Idle
ReadyToConfirm
ChoosingTarget
ordinary Resolving
Blocking Presentation animation
Draw / Shuffle animation
Damage / Status Presentation
CardPlayed animation
Detached card tails
multiple detached cosmetics
```

The following disable/reject EndTurn:

```text
not PlayerTurn
Outcome != None
Terminal
PresentationUnavailable / unsafe recovery
Battle/authority unavailable
active authoritative mandatory card-selection contract
```

The implementation must not use a generic `bInputLocked`/`Resolving` test as the sole EndTurn enable rule.
Card-selection input and EndTurn input now have different availability contracts.

### 7.3 What counts as mandatory selection

Mandatory selection means the battle has an authoritative decision that must be answered before the player
may leave the turn, for example:

```text
PendingCardSelection
choose one card to exhaust
choose N cards to discard
mandatory choose-from-selection-area contract
```

Presentation animation associated with a selection is not itself the blocker. The authoritative unresolved
selection contract is the blocker.

Once the mandatory selection has been resolved, a trailing visual animation alone does not keep EndTurn
disabled.

`ReadyToConfirm` and `ChoosingTarget` are cancelable transient card-play states, not mandatory selections.

### 7.4 Immediate EndTurn path

On physical EndTurn click:

```text
1. reject if mandatory selection / terminal / unavailable / not player turn
2. clear old BufferedCardSelection
3. cancel transient ReadyToConfirm / ChoosingTarget state through the normal cancel contract if needed
4. re-evaluate EndTurn legality
5. if exact safe EndTurn request is legal now:
      execute normal RequestEndTurn once
   else:
      capture BufferedEndTurn
```

Do not call `SkipPresentation()` merely to make EndTurn legal sooner.

### 7.5 Buffered EndTurn replay rule

A pending EndTurn may execute only when all current authority checks pass:

```text
same exact PresentationSessionToken
same BattleId
Outcome == None
battle is still the same PlayerTurn authority window
no authoritative mandatory selection
no terminal / unavailable / recovery ambiguity
normal Gameplay EndTurn legality passes now
action/resolution boundary is safe for one EndTurn request
```

Then:

```text
Take BufferedEndTurn
clear stored intent FIRST
call normal RequestEndTurn once
```

The request is consumed even if the final normal EndTurn request rejects because authority changed
synchronously during the boundary.

### 7.6 Mandatory selection supersedes old EndTurn

If an authoritative mandatory selection appears before a buffered EndTurn executes:

```text
clear BufferedEndTurn
show/enter the mandatory selection normally
require fresh player action after that selection is resolved
```

An old EndTurn click must never cross a newly introduced mandatory decision boundary.

### 7.7 EndTurn stale/clear boundaries

Clear a pending EndTurn on:

```text
Session replacement
HUD / Controller / Battle replacement
BattleId change
authority transition / DirectBaseline transition where buffered Presentation authority no longer applies
terminal
presentation unavailable / unsafe recovery
mandatory PendingSelection appears
battle leaves PlayerTurn before the buffered request executes
EndTurn executes successfully
runtime G9 disable
Global Skip/recovery if its exact capture authority can no longer be proven
```

Detached cosmetic completion does **not** clear EndTurn.

### 7.8 Presentation must not own EndTurn lock

A pure detached visual never controls EndTurn readiness.

Likewise, a still-Blocking Presentation record may delay **execution** of EndTurn if chronology is not yet at
a safe Gameplay boundary, but the player may still press the EndTurn button and create one pending intent.

This distinction is mandatory:

```text
UI accepts EndTurn intent
!=
Gameplay EndTurn executes immediately
```

---

## 8. Relationship with sealed FastInput

G9 has three different mechanisms:

```text
G8 FastInput card catch-up
→ physical card click chooses Skip of real Blocking chronology
→ exact catch-up retry

G9 buffered card selection
→ physical card click during exact G9 card visual window
→ no Skip
→ exact sealed card-selection replay later

G9 buffered EndTurn
→ physical EndTurn click during same player turn
→ no Skip merely because Presentation exists
→ execute at next legal EndTurn boundary
```

For a physical card click, preserve the reviewed order:

```text
1. normal card selection legal now? → normal selection
2. exact G9 card-buffer window?      → buffered card selection
3. sealed FastInput eligible?        → G8 catch-up path
4. otherwise                          → normal reject/base behavior
```

For a physical EndTurn click, use the EndTurn-specific path in §7.4 rather than routing through FastInput.

A pure detached card tail remains:

```text
HasSkippablePresentationDelay() == false
```

Detached visuals are never a reason to Skip.

---

## 9. Hand structural ownership versus hover affordance

### 9.1 Structural Hand reconciliation

Structural work includes:

```text
authoritative Hand membership
slot/order changes
formal card Widget creation/removal
base fan layout
transition geometry
```

It remains governed by authoritative state/reducer ownership.

Non-Hand publications must not needlessly rebuild formal Hand Widgets.

### 9.2 Hover affordance

Hover-only work includes:

```text
hit region
raise translation
scale
angle
z-order
```

Conceptually split APIs:

```cpp
FanHand->ReconcileLayout(...);
FanHand->UpdateHoverAffordance(...);
```

Hover-only work must not:

```text
rebuild Hand
reorder formal cards
move a transient/detached card
claim PlayArea ownership
mutate Gameplay selected-card state
mutate reducer state
restore a hidden historical source card
```

Only current formal visible Hand cards are hover candidates.

---

## 10. Played-card lifecycle identity and detached visual ownership

G9 separates three identities:

```text
RuntimeId
→ Gameplay card instance

PlayedCardPresentationLifecycleToken
→ one exact CardPlayed -> destination Presentation occurrence

DetachedCardVisualToken
→ one private visual job occurrence
```

### 10.1 Controller-owned lifecycle token

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

Rules:

```text
at most one unresolved played-card lifecycle per RuntimeId
PlayArea destination requires exact unresolved lifecycle correlation
zero/multiple/mismatched matches = historical/recovery failure
correlation survives cosmetic job loss
formal destination commit consumes correlation exactly once
Skip/recovery clears correlations whose chronology is collapsed
```

After formal destination commit, the old cosmetic tail may continue while the same RuntimeId later returns
to Hand and receives a new lifecycle generation.

### 10.2 Detached visual job state is fully per-instance

Conceptual job:

```cpp
struct FDetachedCardVisualJob
{
    FDetachedCardVisualToken Token;
    TObjectPtr<UBattleCardWidget> Widget;

    ECardVisualPhase Phase;
    float PhaseElapsedSeconds = 0.0f;
    float PhaseDurationSeconds = 0.0f;

    FVector2D StartPosition;
    FVector2D PlayAreaPosition;
    FVector2D DestinationPosition;

    float StartScale = 1.0f;
    float EndScale = 1.0f;
    float StartOpacity = 1.0f;
    float EndOpacity = 1.0f;

    bool bDestinationCommitted = false;
};
```

Every animation field required for overlap must be job-local. No two jobs may share singleton elapsed,
geometry, phase, opacity, transform or completion state.

### 10.3 Dedicated DetachedCardVFXHost

Detached card visuals live under a stable non-interactive runtime host:

```text
DetachedCardVFXHost
```

Requirements:

```text
HitTestInvisible
never receives focus
never binds card request delegates
never participates in Hand/Target/Selection hit testing
never counts as formal OV_PlayArea child ownership
```

Geometry is frozen/converted into host-local coordinates before detach.

Unsafe viewport/geometry changes may cancel cosmetics only; formal state remains untouched.

### 10.4 Phase machine

```text
Prepared
→ EnteringPlayArea
→ AtPlayArea
→ DestinationTail
→ Done
```

Destination may formally commit before visual arrival finishes:

```text
Destination commits during EnteringPlayArea
→ store exact pending destination spec
→ finish arrival normally
→ immediately enter DestinationTail
```

If arrival finishes first:

```text
EnteringPlayArea -> AtPlayArea
→ wait visually
→ exact destination formal commit
→ DestinationTail
```

Detached completion cleans its own visual only. It never completes Controller chronology or grants input
readiness.

### 10.5 Formal ownership always wins

If an old visual tail and a new formal owner share the same RuntimeId:

```text
formal owner wins
old exact visual job is retired
old callback becomes no-op
```

The old job must never hide/remove/move the new formal Widget.

### 10.6 GC and cleanup

Use deterministic GC-reachable ownership, for example:

```cpp
UPROPERTY(Transient)
TArray<FDetachedCardVisualJob> DetachedCardVisualJobs;
```

Cleanup is exact/idempotent on:

```text
HUD deactivation/destruction
Session invalidation
Controller/Battle replacement
terminal/unavailable
runtime detached-card disable
Global Skip/backlog collapse
recovery of affected chronology
same-RuntimeId formal collision
unsafe geometry change
```

---

## 11. Canonical card historical semantics

Before card NonBlocking activation, G9 must centralize one Controller-side historical rule for:

```text
CardPlayed
CardZoneChanged
```

Conceptual helpers:

```cpp
bool TryApplyCardPlayedRecord(...);
bool TryApplyCardZoneChangedRecord(...);
```

Historical validity and visual eligibility are separate.

### 11.1 Historical validity

Includes, as applicable:

```text
exact frozen card identity
unique RuntimeId/CardId
exact HandIndexBefore
EnergyBefore / EnergyAfter / CostPaid producer contract
source/target Presentation identity
exact played-card lifecycle correlation
valid zone route
```

Historical invalidity causes recovery, not cosmetic fallback.

### 11.2 Visual eligibility

Includes only visual concerns:

```text
DetachedCardVFXHost exists
CardWidgetClass/resource exists
required cached geometry is valid
coordinate conversion succeeds
phase timings are finite positive
job capacity is available
```

Rules:

```text
historical invalid
→ recovery

historical valid + visual preparation declines BEFORE formal commit
→ sealed Blocking fallback when fallback remains valid

formal commit already happened + visual activation/update fails
→ drop cosmetic only
→ never replay reducer
→ never roll back formal state
```

G9-C migrates the Blocking path onto the canonical semantics before any timing change.

---

## 12. Card NonBlocking transaction strategy

### 12.1 G9-C — ownership migration while still Blocking

Migrate:

```text
singleton NativePlayedCardWidget / singleton animation state
→ exact played-card lifecycle token
→ DetachedCardVFXHost
→ exact job token
→ per-job state
```

Timing remains R8-equivalent Blocking until C passes.

### 12.2 G9-D1 — detach destination tail first

`CardPlayed` remains Blocking and reaches PlayArea normally.

Eligible PlayArea destination:

```text
validate canonical historical record + exact lifecycle
prepare destination visual continuation
pre-commit exact recheck
commit formal destination exactly once
publish snapshot
consume formal played-card lifecycle
post-publication exact recheck
activate private DestinationTail
advance Controller immediately
```

DestinationTail completion affects cosmetics only.

This already permits:

```text
A tail alive
+ next exact player surface ready
→ card input / EndTurn can proceed according to their own authority contracts
→ A tail continues
```

### 12.3 G9-D2 — detach CardPlayed arrival

D2 requires an end-to-end preflight before CardPlayed formal commit:

```text
exact current CardPlayed Record
canonical candidate snapshot
exact Session/Battle/record cursor
one supported future matching PlayArea -> {Discard, Exhaust, Removed} Record already sealed
no ambiguous matching lifecycle
visual host/resources
Hand start geometry
PlayArea geometry
future destination endpoint/spec representable
finite timings/capacity
```

If preflight fails before formal commit:

```text
decline CardPlayed detach
→ sealed Blocking CardPlayed
→ D1 may still detach destination later
```

Detached CardPlayed transaction:

```text
prepare EnteringPlayArea job
canonical candidate reducer
pre-commit exact recheck
formal CardPlayed commit once
create unresolved played lifecycle token
publish
post-publication exact recheck
activate private arrival job
advance Controller immediately
```

Future exact destination:

```text
validate destination + lifecycle
commit destination formal state once
publish
consume formal lifecycle
if visual job exists:
    store pending destination spec
    transition when phase permits
advance Controller immediately
```

The destination never waits for detached arrival.

### 12.4 Visual loss after formal CardPlayed commit

After formal commit:

```text
visual job loss / feature disable / geometry loss
→ may remove cosmetic job
→ must retain Controller lifecycle correlation until destination is consumed/collapsed
→ never resurrect/replay CardPlayed
```

### 12.5 Reentrancy

Every detached formal transaction follows:

```text
pre-commit exact authority check
formal commit exactly once
publication
post-publication exact check
then private visual activation/update
```

Post-commit failure is consumed; it never falls back to reducer replay.

---

## 13. Proposed implementation stages

### G9-A — Buffered Player Intent Foundation (shadow only)

Implement/shadow:

```text
FBufferedCardTargetCredential or equivalent
FBufferedCardIntent
FBufferedEndTurnIntent
single buffered-player-intent owner
card exact-target capture/revalidation
EndTurn same-turn authority/revalidation
intent arbitration
atomic consume helpers
mandatory-selection fencing
stale/clear boundaries
Automation-only shadow evaluation
```

Do not yet:

```text
change production hover
change production EndTurn availability
change Skip behavior
replay buffered input in production
change card visual ownership
```

Stop gate:

```text
no predicted card revision
no shadow Gameplay requests
EndTurn intent cannot cross mandatory-selection or player-turn boundaries
```

### G9-B — Buffered Player Input + Hand Hover

Production UX activation:

```text
dirty-aware stable Hand reconciliation
hover/layout separation
buffered card selection
EndTurn availability independent from ordinary Presentation animation
BufferedEndTurn during ordinary resolving/blocking Presentation
EndTurn from ReadyToConfirm / ChoosingTarget cancels transient selection
EndTurn clears older card buffer
mandatory selection disables EndTurn
```

Expected UX:

```text
A Blocking animation active
→ B hover can work in eligible card window
→ early B selection can buffer without Skip
→ EndTurn can be clicked independently

ordinary resolving active
→ EndTurn button remains usable
→ EndTurn executes at next legal boundary exactly once
```

### G9-C — Canonical Card Reducer + Multi-instance Visual Ownership, still Blocking

Implement:

```text
canonical card historical semantics
played-card lifecycle token
exact lifecycle correlation
DetachedCardVFXHost
visual-job token
GC-safe job container
per-job animation state
Blocking completion adapter
same-RuntimeId protection
```

Timing remains Blocking.

### G9-D1 — NonBlocking destination tails

Detach:

```text
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Pure tail must not own:

```text
card-selection readiness
EndTurn readiness
HasSkippablePresentationDelay
Controller completion
```

### G9-D2 — NonBlocking CardPlayed arrival

Detach CardPlayed with the full end-to-end preflight and pending-destination phase handoff.

### G9-E — Integration / cleanup

Validate:

```text
FastInput coexistence
BufferedEndTurn arbitration with FastInput retry
PendingSelection
ReadyToConfirm
ChoosingTarget
ordinary Resolving
Draw/Shuffle Blocking Presentation with EndTurn available
runtime disable
Global Skip
recovery
HUD/Controller/Battle replacement
DirectBaseline
same-RuntimeId redraw
GC/destruction
viewport changes
G8 DamageNumber coexistence
G6 selection regressions
```

No new feature is introduced here.

### G9-F — Evidence / seal

Only after build + affected Automation + manual PIE + evidence review may G9 become:

```text
G9 — COMPLETE / VALIDATED / SEALED
```

---

## 14. Automation requirements

### 14.1 Buffered card target authority

Prove:

```text
exact PresentationOwned Session required
already-sealed exact target required
no numeric future-revision prediction
exact target replacement makes old intent stale
card absent/unplayable drops
mandatory PendingSelection drops
exact ready replays SelectCard once
reentrant broadcast cannot replay twice
```

### 14.2 EndTurn availability

Prove EndTurn remains available/accepted during the player turn through:

```text
Damage Presentation
Status Presentation
CardPlayed Presentation
PlayArea destination Presentation
Draw Presentation
Shuffle Presentation
ordinary Resolving
pure detached card visuals
multiple detached visuals
ReadyToConfirm
ChoosingTarget
```

Prove it is disabled/rejected for:

```text
mandatory PendingCardSelection
not PlayerTurn
Terminal
Outcome != None
PresentationUnavailable / unsafe recovery
```

### 14.3 Buffered EndTurn execution

```text
Resolving + EndTurn click
→ no Skip
→ one BufferedEndTurn
→ next legal boundary executes RequestEndTurn once

Blocking Presentation + EndTurn click
→ animation is not skipped merely for EndTurn
→ EndTurn eventually executes once

Detached cosmetic only + legal EndTurn
→ EndTurn executes immediately; cosmetic may continue
```

### 14.4 EndTurn versus transient card states

```text
ReadyToConfirm + EndTurn
→ transient card selection canceled
→ EndTurn executes/buffers
→ no later Confirm

ChoosingTarget + EndTurn
→ target/card selection canceled
→ EndTurn executes/buffers
→ no later target request
```

### 14.5 EndTurn versus buffered card input

```text
BufferedCardSelection exists
→ physical EndTurn accepted
→ card buffer cleared
→ card never replays later
→ EndTurn executes once

BufferedEndTurn exists
→ ordinary card click does not create a new card intent
→ EndTurn remains the accepted terminal turn command
```

If a G8 FastInput card retry is already pending:

```text
accepted EndTurn
→ pending card retry cannot fire after EndTurn acceptance
```

### 14.6 Mandatory selection fencing

```text
mandatory selection already authoritative
→ EndTurn cannot be captured/executed

BufferedEndTurn waiting
→ new mandatory selection becomes authoritative
→ EndTurn intent clears
→ mandatory selection proceeds
→ completing selection does not resurrect old EndTurn
```

### 14.7 EndTurn stale fencing

```text
Session replacement -> drop
Battle replacement -> drop
BattleId change -> drop
terminal -> drop
unavailable -> drop
battle leaves PlayerTurn before replay -> drop
runtime G9 disable -> drop
successful EndTurn -> consumed exactly once
```

### 14.8 Hand stability

```text
non-Hand publication does not needlessly recreate formal Hand Widgets
hover-only update does not perform structural layout
hidden historical source is not hoverable
surviving Hand card hover works in eligible G9 window
```

### 14.9 Card lifecycle / detached ownership

```text
Blocking/detached paths share canonical card history semantics
PlayArea destination requires exact lifecycle token
A/B visual jobs coexist
all animation state is per-job
stale callback is no-op
formal OV_PlayArea unaffected by detached tails
same-RuntimeId redraw retires old visual only
visual loss does not lose required formal lifecycle correlation
```

### 14.10 D1/D2 transactions

D1:

```text
pre-commit visual decline -> Blocking fallback with zero formal side effect
formal destination commits once
post-commit visual failure -> cosmetic drop only
tail completion has no Controller/input callback
```

D2:

```text
no sealed future destination -> detach declines
ambiguous lifecycle -> detach declines
CardPlayed formal commit creates lifecycle
future destination may commit during EnteringPlayArea
pending destination phase handoff is exact
visual loss before destination does not break formal destination commit
```

### 14.11 Recovery/lifecycle

All replacement/recovery/disable/destruction boundaries must leave:

```text
no ghost card
no duplicate card
no stale callback mutation
no stranded played-card lifecycle
no stuck buffered card intent
no stuck BufferedEndTurn
no EndTurn replay after mandatory-selection / turn / session invalidation
```

---

## 15. Manual PIE gates

### 15.1 G9-B — buffered card selection

```text
play A
→ while A Blocking animation is active, hover B
→ B raises/scales
→ click B
→ A is not Skipped
→ B Gameplay does not execute early
→ exact ready surface replays selection once
→ fresh Confirm/Target still required
```

### 15.2 G9-B — EndTurn during ordinary Presentation

Test at least:

```text
Damage animation
CardPlayed animation
Draw animation
Shuffle animation
ordinary resolving
```

For each:

```text
EndTurn control remains usable while still PlayerTurn and no mandatory selection exists
→ click EndTurn
→ current Presentation is not Skipped merely because of EndTurn
→ EndTurn executes at the first legal Gameplay boundary
→ exactly once
```

### 15.3 G9-B — EndTurn from transient card selection

```text
select no-target card -> ReadyToConfirm
→ EndTurn remains available
→ click EndTurn
→ card selection clears
→ turn ends normally

select target card -> ChoosingTarget
→ EndTurn remains available
→ click EndTurn
→ targeting clears
→ turn ends normally
```

### 15.4 G9-B — mandatory selection

```text
trigger a mandatory choose-card effect
→ EndTurn becomes unavailable/rejected
→ complete required selection
→ trailing visual animation alone must not continue to own EndTurn lock
```

### 15.5 G9-D1 — destination-tail overlap

```text
play A
→ destination formally commits
→ A tail continues privately
→ next player input surface becomes available
→ card selection and EndTurn follow their independent authority contracts
→ A tail is not Skipped merely to accept input
```

### 15.6 G9-D2 — detached arrival

```text
A CardPlayed arrival still moving
→ intermediate committed Records continue
→ destination may formally commit before arrival ends
→ A does not teleport/flash back
→ pending destination transitions after arrival
→ EndTurn/card input can be expressed according to authority without cosmetic ownership
```

### 15.7 Same-RuntimeId return

```text
old A tail alive
→ same RuntimeId returns formally to Hand
→ formal Hand owner is correct/interactable
→ old tail retires safely
→ later new play gets a new lifecycle generation
```

---

## 16. Feature disable, Skip and recovery

### 16.1 Runtime G9 disable without authority replacement

```text
keep current PresentationSessionToken
clear BufferedCardSelection
clear BufferedEndTurn
cancel private detached card visual jobs
stop new G9 buffered captures
stop new detached-card transactions
future new card lifecycles use sealed Blocking path
re-evaluate normal input through sealed guards
```

Do not mint a new SessionToken merely because a feature flag changed.

Already-formally-committed detached `CardPlayed` may still have an unresolved Controller lifecycle required
for its future destination. Feature disable may delete its cosmetic job but must retain that formal
correlation until destination chronology is consumed/collapsed.

### 16.2 Global Skip / backlog collapse

A legitimate sealed Skip path:

```text
clears G9 buffered player intents
cancels current-session detached visuals
retires played-card correlations whose chronology is collapsed
preserves/replaces SessionToken according to sealed G8 rules
```

A pure detached visual alone never makes Skip eligible.

### 16.3 Recovery

Recovery clears buffered intents and G9 lifecycle/job state associated with invalidated chronology even when
G8 intentionally preserves the same PresentationSessionToken.

```text
same SessionToken
!=
old G9 buffered intent still valid after chronology recovery
```

---

## 17. Non-goals

G9 v1 does not authorize:

```text
parallel Gameplay resolutions
multiple queued card commands
multiple queued EndTurn commands
buffered Confirm
buffered Cancel
buffered Target click
speculative energy reservation
speculative target reservation
auto-confirm / auto-target from an old card click
numeric future-revision prediction
EndTurn crossing a mandatory-selection boundary
general detached framework for every Presentation Record
detached DrawPile -> Hand card visuals
rewriting G8 DamageNumber ownership
changing Gameplay card-zone authority
using RuntimeId as visual-job occurrence identity
letting detached visuals occupy formal Hand / OV_PlayArea ownership
```

G9 **does** intentionally allow the EndTurn UI/intent to remain available while some v1 Presentation paths
are still Controller-Blocking. That is part of the design, not a contradiction.

---

## 18. Delivery order and stop gates

Recommended order:

```text
G9-A
buffered card target + BufferedEndTurn authority in shadow
→ build + focused Automation

G9-B
production buffered player input + EndTurn contract + Hand hover
→ build + focused Automation + manual PIE

G9-C
canonical card reducer + lifecycle correlation + multi-job host, still Blocking
→ build + R8-focused Automation + parity PIE

G9-D1
destination-tail NonBlocking activation
→ build + D1/G8/FastInput/input regressions + overlap PIE

G9-D2
CardPlayed-arrival NonBlocking + pending-destination handoff
→ build + D2/D1/G8/input regressions + overlap PIE

G9-E
integration cleanup
→ affected integration matrix

G9-F
evidence / seal
```

Hard stop gates:

```text
A -> B
requires exact card target authority + EndTurn same-turn/mandatory-selection fencing proven

B -> C
requires production buffered card/EndTurn arbitration and Hand hover proven without card chronology changes

C -> D1
requires canonical reducer/lifecycle/job ownership proven while still Blocking

D1 -> D2
requires one-way tail completion + formal host isolation proven

D2 -> E
requires destination-before-arrival + feature-disable/recovery + same-RuntimeId ABA coverage proven
```

---

## 19. Current status

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

Final reviewed target:

```text
Gameplay remains serial
Presentation visuals may overlap
ordinary animation does not own EndTurn lock
ordinary Resolving may delay EndTurn execution but not the player's ability to express EndTurn intent
mandatory selection blocks EndTurn
EndTurn clears older buffered card selection
buffered card click remains selection-only
card replay uses exact sealed target authority
EndTurn replay stays within the same authoritative player-turn window
Hand hover is independent from structural reconciliation
detached card visuals are physically and logically private
formal ownership always wins
G8 sealed behavior remains authority outside explicit G9 scope
```

No production C++ change is authorized by this document alone.
