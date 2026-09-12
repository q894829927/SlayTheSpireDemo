# Selection Presentation G9 — Buffered Player Input + Detached Card Presentation

Date: **2026-09-13**

Status: **DESIGN LOCKED / IMPLEMENTATION NOT STARTED / NOT SEALED**

Authority baseline: [`SelectionPresentationG8FSeal.md`](SelectionPresentationG8FSeal.md).

G8 remains the sealed baseline outside the explicit G9 amendments listed in this document. G9 introduces
buffered player intent and detached played-card Presentation without allowing speculative Gameplay or
parallel Gameplay resolutions.

This revision incorporates the final authority amendments required before G9-A:

```text
1. BufferedCardSelection belongs to Presentation target authority.
2. BufferedEndTurn belongs to Gameplay player-turn authority.
3. EndTurn is protected by BattleId + PlayerTurnSerial, not by PresentationSessionToken.
4. DirectBaseline supports BufferedEndTurn because EndTurn is Gameplay-owned.
5. bCanEndTurn keeps its existing immediate-execution meaning; UI intent availability is separate.
6. G9-B intentionally supersedes one narrow G8-B ChoosingTarget/EndTurn interaction rule.
7. BufferedCardSelection is a Presentation-lag buffer only, never a speculative Gameplay-resolution buffer.
8. A physical EndTurn must be accepted against exact turn authority before older transient/card intents are retired.
```

This document locks design contracts only. It does **not** by itself authorize production C++ changes;
implementation still requires an explicit execution request.

---

## 1. Final G9 goal

G9 targets a Slay-the-Spire-like input feel while preserving deterministic Gameplay.

### 1.1 Card-selection experience

```text
play card A
→ A Presentation continues

while A visual is still alive:
→ surviving formal Hand card B can hover / raise normally
→ clicking B does not Skip A merely because A is still visible

IF Gameplay has already committed and sealed the exact next normal player-card surface
BUT Presentation/display has not caught up yet:
→ capture one BufferedCardSelection for B
→ do not execute B Gameplay early

when display reaches that exact sealed surface:
→ revalidate Session / Battle / exact revision / RuntimeId / legality
→ replay SelectCard(B) exactly once if still valid
```

The buffered card click is selection only:

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

During the same authoritative player turn, ordinary Presentation or an ordinary still-running Gameplay
resolution must not prevent the player from expressing an EndTurn intent merely because execution is not
safe yet.

Examples:

```text
DamageNumber still alive
→ EndTurn may be pressed

CardPlayed / discard / exhaust visual still moving
→ EndTurn may be pressed

Draw / Shuffle / Status Presentation still playing
→ EndTurn may be pressed

ActionQueue still resolving an ordinary player-turn resolution
→ EndTurn may be pressed
→ if QueryEndPlayerTurn is temporarily ResolutionBusy, buffer EndTurn for this SAME player turn
```

If EndTurn is immediately legal, submit it normally. Otherwise:

```text
physical EndTurn click
→ prove current PlayerTurnAuthorityToken
→ accept one EndTurn intent for that token
→ do not Skip Presentation merely to make it executable
→ wait for the next legal EndTurn boundary in that SAME player turn
→ revalidate exact turn authority
→ RequestEndPlayerTurn exactly once
```

The primary player-turn exception is authoritative mandatory selection:

```text
choose one card to exhaust
choose N cards to discard
other PendingCardSelection / mandatory choice
→ EndTurn cannot be accepted until that mandatory decision is completed
```

`ReadyToConfirm` and `ChoosingTarget` are cancelable transient card-play states, not mandatory selections.
Under G9-B, EndTurn is allowed to replace them after exact EndTurn intent acceptance.

### 1.3 Core invariant

> **G9 may overlap visual lifetimes and buffer player intent, but authoritative Gameplay resolutions remain serial.**

Visual overlap and buffered intent never authorize speculative Gameplay.

---

## 2. Authority model: Card buffer and EndTurn buffer are different problems

This distinction is the central G9 authority rule.

```text
BufferedCardSelection
→ Presentation target authority problem
→ exact already-sealed future player surface

BufferedEndTurn
→ Gameplay player-turn authority problem
→ same authoritative player turn
```

Do not force the two intents into one credential model.

### 2.1 Presentation authority

Presentation-owned card lag continues to use:

```text
PresentationSessionToken
BattleId
exact sealed target StateRevision
exact card visual capture window
```

DirectBaseline remains intentionally sessionless and does not support Presentation-lag card buffering.

### 2.2 Gameplay player-turn authority

G9 introduces a Gameplay-owned turn identity, conceptually:

```cpp
struct FPlayerTurnAuthorityToken
{
    uint64 BattleId = 0;
    uint64 PlayerTurnSerial = 0;

    bool IsValid() const
    {
        return BattleId > 0 && PlayerTurnSerial > 0;
    }
};
```

`ABattleManager` is the owner of `PlayerTurnSerial`.

Required lifecycle:

```text
StartBattle
→ PlayerTurnSerial = 0

CompletePlayerTurnStart successfully commits BattleState = PlayerTurn
→ increment PlayerTurnSerial exactly once
→ first player turn becomes serial 1

StateRevision changes inside the same player turn
→ PlayerTurnSerial does NOT change

leave PlayerTurn / EnemyTurn / next PlayerTurn
→ next formal CompletePlayerTurnStart increments again
```

If a theoretical unsigned wrap produces zero, skip zero so valid tokens remain non-zero.

The preferred Gameplay API is conceptually:

```cpp
bool TryGetCurrentPlayerTurnAuthorityToken(
    FPlayerTurnAuthorityToken& OutToken) const;
```

It succeeds only while authoritative Gameplay currently owns `EBattleState::PlayerTurn`.

### 2.3 ABA protection

The following must never compare equal:

```text
Battle 31 / PlayerTurn 4
Battle 31 / PlayerTurn 5
```

Therefore an EndTurn captured in an earlier player turn cannot become valid again merely because the same
BattleId later returns to `PlayerTurn`.

Cleanup hooks remain required, but **cleanup is defense-in-depth, not the identity proof**.

### 2.4 Optional Presentation fence for EndTurn

A HUD-side EndTurn intent captured while PresentationOwned may record the current PresentationSessionToken as
optional provenance/stale defense for HUD/controller replacement. It is not the root authority and is not
required in DirectBaseline.

The root replay proof is always:

```text
same exact FPlayerTurnAuthorityToken
```

---

## 3. Blocking semantics after G9

G9 separates four concepts:

```text
A. Controller/Presentation Blocking
B. card-selection availability
C. ability to express EndTurn intent
D. ability to execute EndTurn now
```

These are not equivalent.

A record may remain Controller-Blocking and may still block ordinary card selection while EndTurn remains
pressable.

Example:

```text
DrawPile -> Hand remains Blocking in G9 v1

meaning:
→ Controller still waits for the draw Presentation contract
→ ordinary card-selection surface may remain unavailable

but NOT meaning:
→ EndTurn UI must be disabled solely because draw animation is playing
```

Likewise:

```text
CanAcceptEndTurnIntent == true
CanExecuteEndTurnNow == false
→ accept/buffer EndTurn
```

---

## 4. G8 sealed baseline and explicit G9-B amendment

G8 remains sealed except where G9 explicitly declares a scoped behavioral supersede.

### 4.1 The one intentional G8-B interaction supersede

G8-B sealed this baseline:

```text
ChoosingTarget
→ bCanEndTurn = false
→ RequestEndTurn rejected
→ selected card / target-choice surface remains intact
```

G9-B intentionally supersedes **exactly this EndTurn interaction contract when G9 buffered-player-input is enabled**:

```text
G9 enabled + ChoosingTarget
→ physical EndTurn may be accepted for current PlayerTurnAuthorityToken
→ once accepted, cancel transient selected-card / target-choice state
→ RequestEndTurn now OR store BufferedEndTurn
```

`ReadyToConfirm` follows the same G9 replacement principle.

This does not reopen unrelated G8 contracts.

### 4.2 Fallback behavior

When G9 buffered-player-input behavior is disabled:

```text
ChoosingTarget
→ preserve sealed G8-B EndTurn rejection
```

Therefore the old G8 behavior remains the fallback baseline.

### 4.3 Regression-test migration

A G8-B test whose exact assertion is:

```text
ChoosingTarget -> EndTurn rejected
```

must not be required to pass unchanged under the G9-enabled production configuration.

Instead:

```text
G9 disabled / fallback configuration
→ original G8-B assertion remains PASS

G9 enabled
→ new G9 test proves EndTurn acceptance, transient-choice cancellation and exact once-only execution
```

Historical G8 tests should be retained where practical; do not silently rewrite the meaning of a sealed test
without documenting the configuration change.

---

## 5. G9 v1 scope

### 5.1 Detached card Presentation scope

Reviewed visual candidates:

```text
CardPlayed
PlayArea -> DiscardPile
PlayArea -> ExhaustPile
PlayArea -> RemovedPile
```

Activation remains staged:

```text
G9-D1
→ detach PlayArea -> destination tails
→ CardPlayed arrival remains Blocking

G9-D2
→ detach CardPlayed arrival
→ destination may formally commit before visual arrival finishes
```

### 5.2 Presentation paths that remain Blocking in v1

Unless separately amended:

```text
DrawPile -> Hand
Hand -> Discard at turn cleanup
Hand -> Exhaust outside played-card lifecycle
SelectionArea transitions
G6 multi-selection Group
Shuffle
PendingSelection Presentation
TargetChoice Presentation
formal Energy / HP / Block / Status chronology
Terminal / unavailable / recovery
```

Again, Controller-Blocking does not automatically mean EndTurn-intent-disabled.

### 5.3 Buffered player-input scope

G9 v1 supports two mutually exclusive pending G9 player intents:

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

---

## 6. Buffered input owner and arbitration

### 6.1 One pending-intent owner

One battle-HUD input owner owns the pending G9 player intent.

Do not duplicate pending state independently across HUD, ViewModel and Controller.

Responsibilities are split:

```text
BattleManager
→ Gameplay player-turn authority
→ QueryEndPlayerTurn / RequestEndPlayerTurn

PresentationController
→ exact Presentation target credentials for BufferedCardSelection

ViewModel
→ current displayed interaction state and normal card-selection contract

G9 input owner
→ one pending physical player intent + arbitration
```

### 6.2 Global priority

Once a physical EndTurn intent has been accepted for exact turn authority:

```text
BufferedEndTurn
> BufferedCardSelection
> pending G8 FastInput card retry
```

This means accepted EndTurn retires older card-oriented future work before that work can fire.

### 6.3 EndTurn must be accepted before destructive retirement

The transaction ordering is frozen:

```text
1. prove current EndTurn intent can be accepted for current PlayerTurnAuthorityToken
2. capture exact FPlayerTurnAuthorityToken
3. only after acceptance, atomically retire:
     - BufferedCardSelection
     - pending G8 FastInput card retry owned by the same player-input window
4. cancel ReadyToConfirm / ChoosingTarget transient card state if present
5. revalidate the captured FPlayerTurnAuthorityToken
6. QueryEndPlayerTurn
7. if Allowed:
     RequestEndPlayerTurn exactly once
   if temporary ResolutionBusy while same turn authority remains:
     store BufferedEndTurn
   otherwise:
     consume/reject the accepted intent according to exact failure policy
```

Do not clear a player's current transient card selection before proving that the physical EndTurn intent can
actually be accepted by the current player-turn authority.

### 6.4 Accepted BufferedEndTurn is decisive

Once `BufferedEndTurn` exists:

```text
new ordinary card selections are not accepted/buffered
new G8 FastInput retry must not be scheduled from a later ordinary card click
```

until EndTurn executes or the exact EndTurn authority becomes stale/invalid.

### 6.5 Event-driven evaluation

Buffered intent evaluation is driven by authoritative Gameplay/Presentation/ViewModel/readiness transitions.

It is not driven by:

```text
NativeTick polling for Gameplay readiness
DamageNumber completion
DetachedCardVisualJob completion
arbitrary cosmetic timer expiry
```

Cosmetic NativeTick may animate private visual jobs only.

---

## 7. Buffered CardSelection authority

### 7.1 Definition: Presentation-lag only

This sentence is normative:

> **BufferedCardSelection is a Presentation-lag buffer, not a speculative Gameplay-resolution buffer.**

In Chinese:

> **BufferedCardSelection 只缓冲“Gameplay 已经提交并封存、但 Presentation 尚未显示到位”的玩家选牌输入；它不预测未来 Gameplay 会到达什么状态。**

Therefore the following is forbidden:

```text
ActionQueue still busy
+
no exact future normal-player target has been sealed yet
+
player clicks card B
→ keep B around because it will probably become playable later   // FORBIDDEN
```

EndTurn may wait across ordinary resolution because it is bound to a same-turn Gameplay token. Card selection
must not copy that capability.

### 7.2 Card intent identity

Conceptual state:

```cpp
struct FBufferedCardWindowIdentity
{
    int64 SourceResolutionId = 0;
    int64 SourcePresentationSequence = 0;
    uint64 LocalWindowGeneration = 0;
};

struct FBufferedCardIntent
{
    FPresentationSessionToken SessionToken;
    int64 BattleId = 0;
    int64 ExpectedReadyRevision = 0;
    FBufferedCardWindowIdentity CaptureWindow;
    int32 RuntimeId = INDEX_NONE;
};
```

Exact type nesting may differ, but these responsibilities are required.

### 7.3 Exact target capture

A Controller-owned helper may conceptually provide:

```cpp
bool TryCaptureBufferedCardTarget(
    int32 RequestedRuntimeId,
    FBufferedCardIntent& OutIntent) const;
```

Capture may succeed only if the future replay surface is already an exact committed/sealed fact:

```text
PresentationOwned mode is current
exact current PresentationSessionToken exists
same BattleId
latest frozen Presentation baseline exists
baseline is the exact intended normal player-card target
Outcome == None
BattleState == PlayerTurn
no authoritative mandatory PendingCardSelection owns that target
RequestedRuntimeId exists as the same authoritative card instance in target Hand
exact target can support the normal player-facing card-selection contract
approved G9 card visual capture window exists
no terminal / unavailable / recovery ambiguity
```

Never invent:

```text
CurrentRevision + 1
numeric future revision guesses
"this active envelope will probably end in ready state"
```

### 7.4 Waiting

While display is behind:

```text
same SessionToken
same BattleId
latest sealed target is still exactly ExpectedReadyRevision
→ wait

sealed target changes
Session changes
Battle changes
→ stale; drop
```

Revision numbers are exact identities, not permission ranges. Do not use `<`, `>`, or `>=` to broaden authority.

### 7.5 Exact replay

Replay requires all exact conditions again:

```text
same SessionToken
same BattleId
latest frozen target still exactly ExpectedReadyRevision
current displayed revision == ExpectedReadyRevision
exact player-facing read matches that BattleId + revision
Outcome == None
normal player-card selection surface
no mandatory PendingSelection
RuntimeId remains in current authoritative Hand
same authoritative card instance
normal live QueryCardPlayability passes
no accepted BufferedEndTurn exists
```

Then:

```text
Take buffered card intent
clear stored card intent FIRST
call normal SelectCardByRuntimeId(RuntimeId) exactly once
```

A failed final normal selection does not resurrect the old click.

### 7.6 Repeated card clicks

Every physical card click re-captures authority.

```text
same exact Session/Battle/ExpectedReadyRevision/CaptureWindow
→ RuntimeId-only replacement may be allowed

different valid credential
→ replace the entire buffered-card intent
```

Never bind a new RuntimeId to an old credential accidentally.

### 7.7 Card-buffer windows

Case A — exact normal card surface already ready:

```text
normal SelectCard now
no buffer
```

Case B — exact approved G9 card visual anchor + exact sealed target already exists:

```text
capture BufferedCardSelection
no Skip
```

The anchor may be either:

```text
approved Blocking played-card Presentation window
or
approved current-session detached played-card visual job
```

Intermediate committed Records such as Damage/Status may continue normally after capture, but the exact target
revision must have been sealed before capture.

Case C — no sealed target / no approved anchor / authority ambiguous:

```text
do not create BufferedCardSelection
preserve normal FastInput/reject behavior for that NEW physical click
```

### 7.8 Card-intent stale boundaries

Clear/drop on:

```text
Session replacement
HUD / Controller / Battle replacement
BattleId change
DirectBaseline transition
Presentation unavailable
terminal
mandatory PendingSelection boundary
Global Skip / backlog collapse
recovery involving target chronology
runtime G9 disable
sealed target replacement
accepted EndTurn
card leaves Hand / becomes illegal at replay
```

---

## 8. EndTurn authority

### 8.1 `bCanEndTurn` keeps its old meaning

G9 must not silently redefine existing `bCanEndTurn`.

Its established meaning remains close to:

```text
this current exact displayed/live surface can execute RequestEndTurn now
```

It may continue to be derived from current snapshot/live `QueryEndPlayerTurn()` semantics.

G9 introduces a separate concept:

```text
CanAcceptEndTurnIntent
→ UI may accept the physical EndTurn press for the current Gameplay player turn

CanExecuteEndTurnNow
→ Gameplay QueryEndPlayerTurn currently allows immediate execution
```

Recommended API concept:

```cpp
bool CanAcceptEndTurnIntent() const;
bool CanExecuteEndTurnNow() const;
```

Exact placement may differ.

Under G9-enabled UI, the EndTurn button must not use generic `bInputLocked` or `bCanEndTurn` as the sole
enable rule.

### 8.2 EndTurn intent identity

Conceptual state:

```cpp
struct FBufferedEndTurnIntent
{
    FPlayerTurnAuthorityToken Turn;
    int64 CaptureStateRevision = 0; // diagnostic/provenance only
    uint64 LocalIntentGeneration = 0;

    // Optional only when captured in PresentationOwned mode.
    // Not root authority and absent in DirectBaseline.
    TOptional<FPresentationSessionToken> PresentationFence;
};
```

`CaptureStateRevision` is not an ExpectedReadyRevision and must not authorize range replay.

### 8.3 PresentationOwned and DirectBaseline both support EndTurn buffering

```text
BufferedCardSelection
→ PresentationOwned only

BufferedEndTurn
→ PresentationOwned + DirectBaseline
```

Reason:

```text
Card buffer asks: which exact sealed Presentation target may this card select on?
EndTurn asks: is this still the exact same authoritative player turn?
```

DirectBaseline is sessionless but still has Gameplay `BattleId + PlayerTurnSerial` authority.

Do not fabricate a PresentationSessionToken for DirectBaseline.

### 8.4 CanAcceptEndTurnIntent

During authoritative Gameplay `PlayerTurn`, EndTurn intent may be accepted through:

```text
ordinary Idle
ReadyToConfirm
ChoosingTarget
ordinary player-turn ResolutionBusy
Blocking Presentation
Draw / Shuffle Presentation
Damage / Status Presentation
CardPlayed Presentation
Detached card tails
multiple detached cosmetics
DirectBaseline ordinary resolving
```

Reject acceptance for:

```text
no valid current FPlayerTurnAuthorityToken
not PlayerTurn
Outcome != None / terminal Gameplay
PresentationUnavailable / unsafe recovery where UI cannot safely preserve command identity
Battle authority unavailable
active authoritative mandatory selection
already accepted BufferedEndTurn for same input owner
```

### 8.5 Mandatory selection

Mandatory selection means Gameplay owns an unresolved decision that must be answered before leaving the turn,
for example:

```text
PendingCardSelection
choose one card to exhaust
choose N cards to discard
mandatory choose-from-selection-area contract
```

Presentation associated with the selection is not itself the blocker. The unresolved authoritative decision
is the blocker.

`ReadyToConfirm` and `ChoosingTarget` remain cancelable transient card-play states.

### 8.6 Physical EndTurn transaction

Normative order:

```text
1. query/prove current FPlayerTurnAuthorityToken
2. prove CanAcceptEndTurnIntent for that exact token
3. capture the token locally; EndTurn intent is now accepted
4. retire old BufferedCardSelection
5. retire pending G8 FastInput retry that could fire after this accepted EndTurn
6. cancel ReadyToConfirm / ChoosingTarget transient state through normal cancel semantics
7. revalidate the captured FPlayerTurnAuthorityToken exactly
8. QueryEndPlayerTurn
9. branch:

   Allowed
   → Take/consume local intent
   → RequestEndPlayerTurn exactly once

   ResolutionBusy AND same exact turn token still current AND no mandatory selection appeared
   → store one BufferedEndTurn

   WrongTurn / BattleEnded / ResolutionFaulted / InvalidBattle / turn-token mismatch / mandatory selection
   → consume/drop EndTurn intent
   → do not resurrect retired card intent
```

The key safety rule is:

> **No destructive cancellation of the player's previous transient card state occurs until the EndTurn intent has first been accepted against exact player-turn authority.**

Do not call `SkipPresentation()` merely to make EndTurn executable sooner.

### 8.7 Buffered EndTurn replay

Replay may execute only if:

```text
same exact FPlayerTurnAuthorityToken
Outcome == None
BattleState still PlayerTurn
no authoritative mandatory selection
no terminal / unsafe recovery ambiguity
QueryEndPlayerTurn().bAllowed now
```

An optional Presentation fence may additionally retire the intent after HUD/controller replacement, but it
cannot make a stale turn token valid.

Then:

```text
Take BufferedEndTurn
clear stored intent FIRST
RequestEndPlayerTurn exactly once
```

If authority changes synchronously during the final request, consume the old intent; do not retry it into a
new player turn.

### 8.8 Mandatory selection kills old EndTurn

If a mandatory selection becomes authoritative before replay:

```text
clear BufferedEndTurn
enter/show mandatory selection normally
require fresh player action after that selection is resolved
```

An old EndTurn never crosses a new mandatory decision boundary.

### 8.9 EndTurn stale/clear boundaries

Clear/drop on:

```text
FPlayerTurnAuthorityToken mismatch
Battle replacement / BattleId change
HUD/input-owner destruction or replacement
terminal / Outcome != None
PresentationUnavailable / unsafe recovery
mandatory selection appears
battle leaves PlayerTurn before execution
successful EndTurn execution
runtime G9 disable
```

A PresentationSessionToken change may also clear a PresentationOwned UI intent as a defensive UI-lifecycle
fence, but Session equality alone never authorizes replay.

Detached cosmetic completion does not clear EndTurn.

### 8.10 UI acceptance is not immediate Gameplay execution

This distinction is mandatory:

```text
CanAcceptEndTurnIntent
!=
CanExecuteEndTurnNow
```

A Controller-Blocking Presentation may delay actual EndTurn execution while the button remains usable.

---

## 9. Relationship with sealed FastInput

G9 and G8 FastInput have different meanings:

```text
G8 FastInput card catch-up
→ new card click chooses to Skip real Blocking chronology
→ exact catch-up retry

G9 BufferedCardSelection
→ Presentation already has an exact sealed card-selection target
→ no Skip
→ replay SelectCard when display catches up

G9 BufferedEndTurn
→ same Gameplay player-turn command
→ no Skip merely because Presentation exists
→ execute when QueryEndPlayerTurn becomes legal in same turn
```

For a new physical card click:

```text
1. accepted BufferedEndTurn exists?   → reject new ordinary card future intent
2. normal card selection legal now?  → normal selection
3. exact G9 card-buffer window?       → BufferedCardSelection
4. sealed FastInput eligible?         → preserve G8 FastInput catch-up
5. otherwise                          → normal reject/base behavior
```

For a new physical EndTurn click, use §8.6. Once accepted, retire older card buffer and pending FastInput
retry before either can fire.

A pure detached card tail remains:

```text
HasSkippablePresentationDelay() == false
```

Detached visuals are never a reason to Skip.

---

## 10. Hand structural ownership versus hover affordance

### 10.1 Structural Hand reconciliation

Structural work includes:

```text
authoritative Hand membership
slot/order changes
formal card Widget creation/removal
base fan layout
transition geometry
```

It remains authoritative reducer/Presentation work.

Non-Hand ViewModel publications must not needlessly destroy/recreate formal Hand Widgets.

### 10.2 Hover affordance

Hover-only work includes:

```text
hit region
raise translation
scale
angle
z-order
```

Conceptual split:

```cpp
FanHand->ReconcileLayout(...);
FanHand->UpdateHoverAffordance(...);
```

Hover-only work must not:

```text
rebuild Hand
reorder formal cards
move transient/detached card ownership
claim PlayArea ownership
mutate selected-card Gameplay state
mutate reducer state
restore a hidden historical source card
```

Only current visible formal Hand cards are hover candidates.

---

## 11. Played-card lifecycle identity and detached visual ownership

G9 separates three card identities:

```text
RuntimeId
→ Gameplay card instance

PlayedCardPresentationLifecycleToken
→ one exact CardPlayed -> destination Presentation occurrence

DetachedCardVisualToken
→ one private visual job occurrence
```

They are not interchangeable.

### 11.1 Controller-owned played-card lifecycle token

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
correlation survives cosmetic visual-job loss
formal destination commit consumes correlation exactly once
Skip/recovery clears correlations whose chronology is collapsed
```

After formal destination commit, the old cosmetic tail may continue while the same RuntimeId later returns
to Hand and receives a new lifecycle generation.

### 11.2 Detached visual token and per-job state

Visual job identity extends the played-card occurrence with an exact local visual generation. RuntimeId alone
never addresses a visual job.

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

Every animation field needed for overlap is job-local. No two jobs share singleton elapsed/geometry/phase/
opacity/transform/completion state.

### 11.3 Dedicated `DetachedCardVFXHost`

Detached visuals live under a stable non-interactive host:

```text
DetachedCardVFXHost
```

Requirements:

```text
HitTestInvisible
never receives focus
never binds card-request delegates
never participates in Hand / Target / Selection hit testing
never counts as formal OV_PlayArea child ownership
```

Geometry is frozen/converted to host-local coordinates before detach.

Unsafe geometry changes may cancel cosmetics only; formal state remains untouched.

### 11.4 Phase machine

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

Detached completion cleans only its own cosmetic state. It never completes Controller chronology or grants
Gameplay readiness.

### 11.5 Formal ownership always wins

If an old tail and a new formal owner share the same RuntimeId:

```text
formal owner wins immediately
old exact visual job retires
old callback becomes no-op
```

Old visual work must never hide/remove/move the new formal Widget.

### 11.6 GC and cleanup

Use deterministic GC-reachable ownership, conceptually:

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

## 12. Canonical card historical semantics

Before card NonBlocking activation, G9 centralizes one Controller-side historical rule for:

```text
CardPlayed
CardZoneChanged
```

Conceptual helpers:

```cpp
bool TryApplyCardPlayedRecord(...);
bool TryApplyCardZoneChangedRecord(...);
```

Historical validity and visual eligibility remain separate.

### 12.1 Historical validity

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

### 12.2 PlayArea destination correlation

The frozen snapshot does not itself provide authoritative PlayArea card ownership sufficient to correlate a
played-card occurrence.

Therefore `PlayArea -> destination` additionally requires the exact unresolved
`FPlayedCardPresentationLifecycleToken`.

Missing/wrong lifecycle correlation is historical/recovery failure, not a visual eligibility decline.

### 12.3 Visual eligibility

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

historical valid + visual prepare declines BEFORE formal commit
→ sealed Blocking fallback when fallback remains valid

formal commit already happened + visual activation/update fails
→ drop cosmetic only
→ never replay reducer
→ never roll back formal state
```

G9-C migrates the Blocking path onto canonical semantics before any timing change.

---

## 13. Card NonBlocking transaction strategy

### 13.1 G9-C — ownership migration while still Blocking

Migrate:

```text
singleton NativePlayedCardWidget / singleton animation state
→ exact played-card lifecycle token
→ DetachedCardVFXHost
→ exact visual-job token
→ per-job state
```

Timing remains R8-equivalent Blocking until C passes.

### 13.2 G9-D1 — detach destination tail first

`CardPlayed` remains Blocking and reaches PlayArea normally.

Eligible PlayArea destination:

```text
validate canonical historical record + exact unresolved lifecycle
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
+ next player input authority becomes available
→ card selection / EndTurn follow their independent authority contracts
→ A tail continues
```

### 13.3 G9-D2 — detach CardPlayed arrival

D2 requires end-to-end preflight before CardPlayed formal commit:

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
→ D1 may still detach the destination later
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

The destination Record never waits for detached arrival.

### 13.4 Visual loss after formal CardPlayed commit

After formal commit:

```text
visual job loss / feature disable / geometry loss
→ may remove cosmetic job
→ must retain Controller lifecycle correlation until destination is consumed/collapsed
→ never resurrect/replay CardPlayed
```

### 13.5 Reentrancy

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

## 14. Proposed implementation stages

### G9-A — Authority Foundation (shadow only)

G9-A now freezes two independent authority tracks.

Implement/shadow:

```text
Gameplay FPlayerTurnAuthorityToken
Battle-owned PlayerTurnSerial
TryGetCurrentPlayerTurnAuthorityToken or equivalent
CanAcceptEndTurnIntent vs CanExecuteEndTurnNow split
BufferedEndTurn authority for PresentationOwned + DirectBaseline
FBufferedCardIntent exact Presentation target authority
single buffered-player-intent owner
EndTurn > CardSelection > pending FastInput arbitration
accept-before-retire EndTurn transaction
mandatory-selection fencing
stale/clear boundaries
Automation-only shadow evaluation
```

Do not yet:

```text
change production hover
change production EndTurn button behavior
change G8 ChoosingTarget behavior
change Skip behavior
replay buffered input in production
change card visual ownership
```

A stop gate:

```text
PlayerTurnSerial increments exactly once per formal PlayerTurn entry
old-turn token cannot ABA-match a later PlayerTurn
DirectBaseline EndTurn shadow works without SessionToken
card buffer cannot capture without an already-sealed exact target
no shadow Gameplay request is emitted
```

### G9-B — Buffered Player Input + Hand Hover

Production activation:

```text
dirty-aware stable Hand reconciliation
hover/layout separation
BufferedCardSelection production replay
EndTurn UI uses CanAcceptEndTurnIntent, not generic bInputLocked/bCanEndTurn alone
BufferedEndTurn during ordinary ResolutionBusy / Blocking Presentation
DirectBaseline BufferedEndTurn
G9 scoped ChoosingTarget/ReadyToConfirm supersede
EndTurn accepted before transient/card/FastInput retirement
mandatory selection disables/kills EndTurn
```

Expected UX:

```text
A Blocking animation active
→ B hover works in eligible exact card window
→ early B may buffer only if sealed target already exists
→ EndTurn may be expressed independently

ordinary same-turn resolution active
→ EndTurn remains pressable
→ EndTurn executes at next legal same-turn boundary exactly once
```

### G9-C — Canonical card reducer + multi-instance visual ownership, still Blocking

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

A pure tail must not own:

```text
card-selection readiness
EndTurn intent availability
EndTurn immediate-execution legality
HasSkippablePresentationDelay
Controller completion
```

### G9-D2 — NonBlocking CardPlayed arrival

Detach CardPlayed with full end-to-end preflight and pending-destination phase handoff.

### G9-E — Integration / cleanup

Validate:

```text
G8 FastInput coexistence
EndTurn arbitration with pending FastInput retry
PresentationOwned + DirectBaseline EndTurn
PlayerTurn token rollover / ABA rejection
PendingSelection
ReadyToConfirm
ChoosingTarget scoped G8 supersede
ordinary Resolving
Draw/Shuffle Blocking Presentation with EndTurn intent available
runtime disable
Global Skip
recovery
HUD/Controller/Battle replacement
same-RuntimeId redraw
GC/destruction
viewport changes
G8 DamageNumber coexistence
G6 selection regressions
```

No new feature is introduced in E.

### G9-F — Evidence / seal

Only after build + affected Automation + manual PIE + evidence review may G9 become:

```text
G9 — COMPLETE / VALIDATED / SEALED
```

---

## 15. Automation requirements

### 15.1 Player-turn authority token

Prove:

```text
StartBattle resets PlayerTurnSerial authority
first formal CompletePlayerTurnStart creates serial 1
StateRevision changes inside same player turn do not change PlayerTurnSerial
EnemyTurn -> next PlayerTurn increments serial
old token from PlayerTurn N != token from PlayerTurn N+1
BattleId change invalidates old token
no cleanup omission can make an old token valid in a later player turn
```

### 15.2 EndTurn availability versus execution

Prove separately:

```text
CanAcceptEndTurnIntent
CanExecuteEndTurnNow / bCanEndTurn immediate semantics
```

Cases where intent remains accepted during PlayerTurn even if immediate execution may be false:

```text
Damage Presentation
Status Presentation
CardPlayed Presentation
PlayArea destination Presentation
Draw Presentation
Shuffle Presentation
ordinary ResolutionBusy
pure detached card visuals
multiple detached visuals
ReadyToConfirm
ChoosingTarget under G9 enabled
DirectBaseline ordinary resolving
```

Reject intent for:

```text
mandatory PendingCardSelection
no valid PlayerTurnAuthorityToken
not PlayerTurn
Terminal / Outcome != None
PresentationUnavailable / unsafe recovery
```

### 15.3 DirectBaseline EndTurn

```text
DirectBaseline + same PlayerTurn + QueryEndPlayerTurn Allowed
→ immediate EndTurn

DirectBaseline + same PlayerTurn + ResolutionBusy
→ BufferedEndTurn captured with Gameplay turn token
→ no fabricated SessionToken
→ exact same turn later becomes legal
→ EndTurn executes once
```

### 15.4 EndTurn ABA/stale fencing

```text
buffer EndTurn in PlayerTurn N
→ leave PlayerTurn before replay
→ old intent stale
→ later PlayerTurn N+1 begins
→ old intent cannot execute
```

Also prove:

```text
Battle replacement -> drop
HUD/input-owner replacement -> drop
terminal -> drop
mandatory selection appears -> drop
runtime G9 disable -> drop
successful EndTurn -> consumed once
```

### 15.5 EndTurn accept-before-retire transaction

Prove:

```text
EndTurn authority capture fails
→ existing ReadyToConfirm / ChoosingTarget remains intact
→ existing card buffer is not destructively retired by an unaccepted EndTurn

EndTurn authority capture succeeds
→ retire old card buffer / pending FastInput retry
→ cancel transient selection
→ revalidate exact turn token
→ immediate RequestEndTurn OR BufferedEndTurn
```

### 15.6 G8-B scoped supersede

G9 disabled:

```text
ChoosingTarget -> EndTurn rejected
original G8-B fallback contract remains PASS
```

G9 enabled:

```text
ChoosingTarget + accepted EndTurn
→ target/card transient state canceled
→ EndTurn immediate or buffered
→ no later target request
```

Do the same for `ReadyToConfirm` replacement semantics.

### 15.7 Card buffer is Presentation-lag only

Prove:

```text
exact PresentationOwned Session required
already-sealed exact normal-player target required
no numeric future-revision prediction
ActionQueue busy + no sealed future card target -> NO BufferedCardSelection
exact target replacement makes old intent stale
card absent/unplayable drops
mandatory PendingSelection drops
exact ready replays SelectCard once
reentrant broadcast cannot replay twice
DirectBaseline does not create BufferedCardSelection
```

### 15.8 EndTurn versus card/FastInput arbitration

```text
BufferedCardSelection exists
→ physical EndTurn is accepted
→ card buffer retired
→ card never replays

pending G8 FastInput retry exists
→ accepted EndTurn retires retry
→ retry cannot fire after EndTurn acceptance

BufferedEndTurn exists
→ ordinary card click does not create new card future intent
```

### 15.9 Mandatory selection fencing

```text
mandatory selection already authoritative
→ EndTurn cannot be accepted

BufferedEndTurn waiting
→ new mandatory selection becomes authoritative
→ EndTurn clears
→ mandatory selection proceeds
→ finishing selection does not resurrect old EndTurn
```

### 15.10 Hand stability

```text
non-Hand publication does not needlessly recreate formal Hand Widgets
hover-only update does not perform structural layout
hidden historical source is not hoverable
surviving formal Hand card hover works in eligible G9 window
```

### 15.11 Card lifecycle / detached ownership

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

### 15.12 D1/D2 transactions

D1:

```text
pre-commit visual decline -> Blocking fallback with zero formal side effect
formal destination commits once
post-commit visual failure -> cosmetic drop only
tail completion has no Controller/input callback
```

D2:

```text
no sealed future supported destination -> detach declines
ambiguous lifecycle -> detach declines
CardPlayed formal commit creates lifecycle
future destination may commit during EnteringPlayArea
pending destination phase handoff is exact
visual loss before destination does not break formal destination commit
```

### 15.13 Recovery/lifecycle

All replacement/recovery/disable/destruction boundaries must leave:

```text
no ghost card
no duplicate card
no stale callback mutation
no stranded played-card lifecycle
no stuck BufferedCardSelection
no stuck BufferedEndTurn
no old-turn EndTurn replay
no stale FastInput retry after accepted EndTurn
```

---

## 16. Manual PIE gates

### 16.1 G9-B — buffered card selection

```text
play A
→ while A Blocking animation is active, hover B
→ B raises/scales
→ click B only when exact target is already sealed but display is behind
→ A is not Skipped
→ B Gameplay does not execute early
→ exact ready surface replays selection once
→ fresh Confirm/Target still required
```

Negative check:

```text
ActionQueue is still resolving and no exact future card-selection target is sealed
→ click B
→ G9 does NOT retain a speculative BufferedCardSelection
```

### 16.2 G9-B — EndTurn during ordinary Presentation/resolution

Test at least:

```text
Damage animation
CardPlayed animation
Draw animation
Shuffle animation
ordinary same-turn ResolutionBusy
```

For each:

```text
valid PlayerTurnAuthorityToken exists
no mandatory selection
→ EndTurn control accepts physical click
→ current Presentation is not Skipped merely because of EndTurn
→ if immediate query is busy, one BufferedEndTurn is stored
→ EndTurn executes at first legal boundary in the SAME player turn
→ exactly once
```

### 16.3 G9-B — DirectBaseline EndTurn

```text
DirectBaseline / no SessionToken
→ same PlayerTurn token exists
→ ordinary resolution busy
→ click EndTurn
→ buffer by Gameplay turn authority
→ later same turn legal
→ execute once
```

### 16.4 G9-B — transient card selection supersede

```text
G9 enabled
select no-target card -> ReadyToConfirm
→ click EndTurn
→ EndTurn authority accepted first
→ transient card selection clears
→ turn ends now/buffered

select target card -> ChoosingTarget
→ click EndTurn
→ EndTurn authority accepted first
→ targeting clears
→ turn ends now/buffered
```

Fallback check:

```text
G9 buffered-player-input disabled
→ ChoosingTarget EndTurn behavior remains sealed G8-B rejection
```

### 16.5 G9-B — mandatory selection

```text
trigger mandatory choose-card effect
→ EndTurn cannot be accepted
→ complete required selection
→ trailing visual animation alone does not continue to own EndTurn lock
```

### 16.6 G9-B — player-turn ABA

```text
capture BufferedEndTurn in PlayerTurn N
→ force/allow transition out of that PlayerTurn before replay
→ begin PlayerTurn N+1
→ old EndTurn never executes
```

### 16.7 G9-D1 — destination-tail overlap

```text
play A
→ destination formally commits
→ A tail continues privately
→ next player input authority becomes available
→ card selection and EndTurn follow independent contracts
→ A tail is not Skipped merely to accept input
```

### 16.8 G9-D2 — detached arrival

```text
A CardPlayed arrival still moving
→ intermediate committed Records continue
→ destination may formally commit before arrival ends
→ A does not teleport/flash back
→ pending destination transitions after arrival
→ EndTurn/card input follows its own authority without cosmetic ownership
```

### 16.9 Same-RuntimeId return

```text
old A tail alive
→ same RuntimeId returns formally to Hand
→ formal Hand owner is correct/interactable
→ old tail retires safely
→ later new play gets a new lifecycle generation
```

---

## 17. Feature disable, Skip and recovery

### 17.1 Runtime G9 disable without Gameplay authority replacement

```text
clear BufferedCardSelection
clear BufferedEndTurn
cancel private detached-card visual jobs
stop new G9 buffered captures
stop new detached-card transactions
future new card lifecycles use sealed Blocking path
re-evaluate fallback input through sealed guards
```

Do not mutate `PlayerTurnSerial` merely because a UI feature flag changed.

Do not mint a new PresentationSessionToken merely because G9 is disabled.

Already-formally-committed detached `CardPlayed` may still have unresolved Controller lifecycle correlation
required for its future destination. Feature disable may delete cosmetics but must retain formal correlation
until destination chronology is consumed/collapsed.

### 17.2 Global Skip / backlog collapse

A legitimate sealed Skip path:

```text
clears affected G9 Presentation-lag card intent
cancels current-session detached visuals
retires played-card correlations whose chronology is collapsed
preserves/replaces SessionToken according to sealed G8 rules
```

For EndTurn, Skip is not the root invalidation rule. The EndTurn intent remains valid only if the same exact
`FPlayerTurnAuthorityToken` and all EndTurn acceptance conditions still hold after the operation; otherwise it
is cleared.

A pure detached visual alone never makes Skip eligible.

### 17.3 Recovery

Recovery clears G9 state associated with invalidated chronology/UI authority.

For card intent:

```text
same SessionToken after recovery
!=
old card target still valid
```

For EndTurn:

```text
same BattleId after recovery
!=
same player-turn authority
```

Exact `FPlayerTurnAuthorityToken` must still match, and unsafe recovery may clear the UI intent defensively.

---

## 18. Non-goals

G9 v1 does not authorize:

```text
parallel Gameplay resolutions
multiple queued card commands
multiple queued EndTurn commands
buffered Confirm
buffered Cancel
buffered Target click
speculative card-selection buffering across unsealed Gameplay results
speculative energy reservation
speculative target reservation
auto-confirm / auto-target from an old card click
numeric future-revision prediction
EndTurn crossing PlayerTurnSerial boundaries
EndTurn crossing a mandatory-selection boundary
general detached framework for every Presentation Record
detached DrawPile -> Hand card visuals
rewriting G8 DamageNumber ownership
changing Gameplay card-zone authority
using RuntimeId as visual-job occurrence identity
letting detached visuals occupy formal Hand / OV_PlayArea ownership
silently redefining bCanEndTurn from immediate legality into UI intent availability
```

G9 intentionally permits EndTurn intent while some v1 Presentation paths remain Controller-Blocking.

---

## 19. Delivery order and stop gates

Recommended order:

```text
G9-A
Gameplay PlayerTurnAuthorityToken
+ EndTurn express/execute split
+ DirectBaseline EndTurn shadow
+ exact Presentation-lag card target shadow
+ arbitration shadow
→ build + focused Automation

G9-B
production buffered player input
+ scoped G8-B EndTurn supersede
+ Hand hover
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
requires:
- PlayerTurnAuthorityToken ABA proof
- CanAcceptEndTurnIntent / CanExecuteEndTurnNow separation
- DirectBaseline BufferedEndTurn proof
- card Presentation-lag-only capture proof
- accept-before-retire shadow transaction proof

B -> C
requires:
- production EndTurn/card/FastInput arbitration
- G8-B scoped supersede migration
- mandatory-selection fencing
- Hand hover stability

C -> D1
requires canonical reducer/lifecycle/job ownership proven while still Blocking

D1 -> D2
requires one-way tail completion + formal host isolation proven

D2 -> E
requires destination-before-arrival + feature-disable/recovery + same-RuntimeId ABA coverage proven
```

---

## 20. Current status and locked principles

```text
G8    — COMPLETE / VALIDATED / SEALED
G9    — DESIGN LOCKED / IMPLEMENTATION NOT STARTED / NOT SEALED
G9-A  — NOT STARTED
G9-B  — NOT STARTED
G9-C  — NOT STARTED
G9-D1 — NOT STARTED
G9-D2 — NOT STARTED
G9-E  — NOT STARTED
G9-F  — NOT STARTED
```

Locked principles:

```text
Gameplay remains serial
Presentation visuals may overlap

BufferedCardSelection belongs to Presentation target authority
BufferedCardSelection is Presentation-lag only
BufferedCardSelection requires an already-sealed exact target
DirectBaseline does not fabricate card Presentation credentials

BufferedEndTurn belongs to Gameplay player-turn authority
EndTurn root identity = BattleId + PlayerTurnSerial
DirectBaseline and PresentationOwned both support EndTurn buffering
PresentationSessionToken is optional EndTurn UI provenance, not root authority

bCanEndTurn keeps immediate-execution semantics
CanAcceptEndTurnIntent is separate from CanExecuteEndTurnNow
ordinary Resolving may delay EndTurn execution without blocking intent expression
mandatory authoritative selection blocks/kills EndTurn

accepted EndTurn
> older BufferedCardSelection
> pending G8 FastInput retry

EndTurn authority is proven before destructive retirement of prior transient/card intent

G9-B explicitly supersedes only the configured ChoosingTarget/EndTurn G8-B interaction rule
G9-disabled fallback preserves sealed G8-B behavior

Hand hover is independent from structural reconciliation
detached card visuals are physically and logically private
formal ownership always wins
canonical card historical semantics precede NonBlocking timing changes
D1 precedes D2
G8 remains authoritative outside explicitly listed G9 amendments
```

No production C++ change is authorized by this document alone.
