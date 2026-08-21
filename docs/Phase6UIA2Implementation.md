# Phase 6UI-A2 — Basic Committed Presentation

Status: **UI-A2A/A2B/A2C C++ VALIDATED / A2D DESIGN LOCKED**.

UI-A2 replaces the UI-A0/UI-A1 immediate/no-op presentation catch-up boundary with deterministic playback of already-committed gameplay facts. It does not make `BattleActionQueue`, `BattleState` or authoritative gameplay wait for animation.

This document is the base implementation contract. UI-A2A establishes transport, resolution, freezing, failure and playback-safety infrastructure; A2B/A2C add committed Damage/Block and Card/Energy/Zone/Shuffle facts. The more specific A2D Status/Terminal contract is `docs/Phase6UIA2DImplementation.md`.

### Current implementation / validation status

The repository owner confirmed the UE5.8 affected aggregate run for the current UI-A2A/A2B/A2C C++ source. Exact evidence is recorded in `docs/Phase6UIA2CValidation.md`.

Current evidence boundary:

```text
UI-A2A C++ / Automation             PASSED 8/8
UI-A2B C++ / Automation             PASSED 8/8
UI-A2C C++ / Automation             PASSED 8/8
Affected aggregate gate             PASSED 77/77
Blueprint A2B/A2C playback          DEFERRED
PIE presentation smoke              DEFERRED
UI-A2D design contract              LOCKED
UI-A2D implementation / validation  NOT STARTED
```

The historical 92/92 owner run and the affected 77/77 A2 aggregate are different configured suites and must not be added together. C++/Automation validation does not claim the deferred Blueprint animation or PIE work.

---

## 1. Required closed loop

```text
Request / System operation
↓
final gameplay validation
↓
BeginResolution(Origin)
↓
prepare / enqueue authoritative work
↓
BattleActionQueue
↓
Gameplay Commit
↓
typed CommitResult / MutationResult
↓
Action / BattleManager adds Source / Reason / Resolution context
↓
optional battle-scoped RecordWriter appends deterministic Presentation Record
↓
Gameplay continues independently
↓
macro flow fully stabilizes
↓
INTERNAL GAMEPLAY-STABLE BOUNDARY
Build exact raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal immutable FPresentationResolutionEnvelope exactly once
↓
release/clear the active Resolution builder immediately
↓
next Resolution is now allowed to Begin
↓
PUBLIC DEFERRED NOTIFICATION BOUNDARY
originating public Request has returned
↓
deliver sealed Envelope to Presentation Coordinator / Controller
↓
notify ordinary OnReadStateReady observers
↓
bounded Envelope queue
↓
play Records in PresentationSequence order
↓
Finished / Skip / timeout / fail-safe
↓
Apply that Envelope.FinalSnapshot
↓
when caught up to newest BattleId/Revision, refresh live input bindings only
↓
unlock input only if authoritative gameplay is request-eligible
```

Durable separation:

```text
Gameplay
= what is true

FPresentationStateSnapshot
= the complete frozen player-facing display state at one exact stable revision

Presentation Record
= one meaningful already-committed historical fact

FPresentationResolutionEnvelope
= exact immutable pairing of Records[] and FinalSnapshot for one Resolution

PresentationController
= when/how sealed facts are shown
```

`BattleActionQueue != PresentationQueue`.

Presentation failure, delay, skip, disablement, missing Blueprint callbacks or backlog pressure must never alter authoritative gameplay results.

A Resolution is sealed synchronously at the internal Gameplay-stable boundary. Delivery of the already-sealed Envelope is a separate deferred public-notification concern. UI callbacks must never be allowed to keep the previous Resolution builder active until the next tick.

---

## 2. Four UI-A2A hard rules

### 2.1 Frozen snapshot is the only historical display input

`FBattleReadSnapshot` remains the current Gameplay-facing coherent read boundary and may contain weak runtime references.

UI-A2 adds exactly one frozen player-facing display model:

```text
FBattleReadSnapshot
= current coherent Gameplay/read state
= may contain weak runtime references

FPresentationStateSnapshot
= complete frozen player-facing values for one exact revision
= used by both Envelope.FinalSnapshot and the immediate latest HUD baseline

BattleHUDViewModel
= copies/applies FPresentationStateSnapshot
```

`FPresentationStateSnapshot` may keep immutable presentation assets such as textures. It must not depend on mutable Gameplay runtime identities such as:

```text
UCardInstance
UStatusInstance
ACombatant
ABattleManager
```

Applying a frozen snapshot must not call current Gameplay APIs to rebuild historical card metadata, status text, playability, target legality, HP/Block/Energy or pile state.

### 2.2 Historical display and live input identity are separate

The current formal Request API still needs runtime objects:

```text
RequestPlayCard(UCardInstance*, ACombatant*)
```

UI-A2A therefore keeps a separate, non-authoritative input binding cache only for the newest caught-up revision:

```text
RuntimeId → TWeakObjectPtr<UCardInstance>
TargetId  → TWeakObjectPtr<ACombatant>
```

Rules:

```text
playing historical Envelope
→ input locked
→ no runtime binding lookup for rendering or historical playback

Controller caught up to newest sealed (BattleId, StateRevision)
→ apply newest Envelope.FinalSnapshot
→ rebuild input bindings through current formal read/query boundary
→ verify binding BattleId/Revision matches displayed latest state
→ only then unlock normal input
```

The binding cache exists only to forward current user intent to formal Gameplay Requests. It is not Gameplay authority. Requests still revalidate authoritative state.

Do not expand UI-A2A into `RequestPlayCardByRuntimeId()` without a later concrete need.

### 2.3 Seal lifecycle and public notification lifecycle are separate

`OnReadStateReady(BattleId, StateRevision)` remains a non-replaying edge meaning that a stable current state can be read. It is not a historical payload.

The internal Gameplay-stable boundary must complete synchronously before another Resolution may begin:

```text
Queue / macro flow becomes fully stable
↓
Build exact raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal active Envelope exactly once
↓
clear/release active builder
↓
append sealed Envelope to the battle-scoped pending-public-delivery FIFO
```

This internal sealing step may happen while the originating public Request is still on the call stack. That is required so a later Debug/API/Automation/System caller cannot begin Resolution N+1 while Resolution N still owns the active builder.

More than one Resolution may Seal before the deferred public callback runs. Therefore this handoff must be a FIFO, not one overwriteable `PendingEnvelope`:

```text
Seal Resolution N
→ PendingPublicDeliveryQueue.Enqueue(Envelope N)
→ release builder
→ Begin/Seal Resolution N+1 is allowed
→ PendingPublicDeliveryQueue.Enqueue(Envelope N+1)
→ deferred boundary delivers N, then N+1
```

The FIFO is battle-scoped and bounded. It preserves `ResolutionId` order, is cleared on battle restart, never delivers an old `BattleId`, and does not coalesce Envelopes merely because current read-state notification advances to a newer `StateRevision`. Overflow is Presentation-only degradation: collapse/skip toward the newest frozen `FinalSnapshot` or disable Presentation for the battle, but never request Gameplay `ResolutionFault`.

Public callbacks are deliberately later:

```text
originating public Request returns AcceptedForResolution / Rejected result
↓
next safe deferred public-notification boundary
↓
drain pending-public-delivery FIFO in ResolutionId order
↓
OnPresentationResolutionReady(each sealed Envelope), when presentation delivery is enabled
↓
OnReadStateReady(BattleId, StateRevision) for ordinary read observers
```

Neither `OnPresentationResolutionReady` nor `OnReadStateReady` may re-enter an accepted public Request before that Request returns.

Display ownership is singular:

```text
Presentation enabled
→ Presenter/Controller drives ViewModel display using frozen snapshots
→ ViewModel must not Pull+Apply live state from OnReadStateReady

Presentation disabled / no Controller
→ apply the same newest frozen baseline immediately at the public presentation boundary
→ no historical playback required
```

`OnReadStateReady` remains useful for ordinary current-state observers, but it must not become a second HUD display driver.

### 2.4 Ordinary validation rejection creates no Resolution; post-validation framework fault does

Normal gameplay validation failure creates no Presentation Resolution:

```text
invalid card / target / energy / turn / battle state
→ Request rejected
→ no BeginResolution
→ no Envelope
```

After final validation succeeds, establish a builder before any preparation/enqueue operation that can produce a framework fault:

```text
validation passes
↓
BeginResolution(Origin)
↓
prepare / atomically enqueue
├── success → normal processing
├── framework fault → preserve committed Records, append ResolutionFault, Seal fault Envelope
└── truly side-effect-free non-framework failure → Abort builder
```

`BattleStart` begins its Resolution before opening/setup work that may produce a framework fault.

---

## 3. Resolution lifecycle

### 3.1 Identity

Every Record belongs to:

```text
BattleId
ResolutionId
PresentationSequence
```

`PresentationSequence` is battle-scoped deterministic ordering and is independent of `StateRevision`.

Do not use UObject address, Widget creation order, delegate registration order, frame timing, animation timing or unordered iteration as presentation order.

### 3.2 Origins

First implementation uses only:

```text
BattleStart
PlayCard
EndTurn
System
```

Do not predeclare AI/Relic/Replay origins until a real caller needs them.

### 3.3 End Turn macro flow

The current complete automatic End Turn progression is one Resolution:

```text
remaining Hand cleanup
→ Player TurnEnded reactions
→ Enemy turn
→ Enemy TurnEnded reactions
→ next PlayerTurnStarting work
→ next player Draw work
→ stable PlayerTurn
```

Do not split this macro flow merely for animation pacing.

### 3.4 Seal-before-next-Begin invariant

There may be at most one active Presentation Resolution builder for the battle.

```text
BeginResolution(N)
↓
Gameplay for N may resolve synchronously inside StartProcessing()
↓
internal stable boundary seals or aborts N
↓
active builder is released
↓
only then may BeginResolution(N+1) succeed
```

The deferred public delivery of Envelope N does not keep Resolution N active. Sealed-but-not-yet-delivered Envelopes are immutable entries in the bounded pending-public-delivery FIFO, not active Recorder builders.

If a caller attempts to begin another Resolution while a builder is still legitimately active, the call must not silently overwrite that builder. The implemented A2A fail-safe rejects the second Begin, clears the stale presentation-only builder, and degrades Presentation for that battle rather than carrying the stale builder into a later Resolution. Gameplay remains unchanged.

The pending-public-delivery FIFO is not a Recorder history database and is separate from the Controller backlog. Its only responsibility is reliable ordered handoff from internal Seal to deferred public broadcast.

### 3.5 Fault lifecycle belongs to UI-A2A

`ResolutionFault` Record lifecycle is infrastructure, not an A2D-only feature.

If a fault occurs after the builder exists:

```text
already committed Records remain
→ append ResolutionFault as the final Record
→ freeze ResolutionFaulted FinalSnapshot
→ Seal once at the internal stable boundary
→ release builder
```

Once `ResolutionFault` has been appended, any later append attempt invalidates the whole unpublished batch. This enforces the append-last invariant instead of publishing a history in which a terminal framework fault is followed by ordinary records.

UI-A2A must implement and test this lifecycle even though the polished/normal visible fault presentation belongs to UI-A2D.

---

## 4. Immutable Resolution Envelope

```text
FPresentationResolutionEnvelope
├── BattleId
├── ResolutionId
├── Origin
├── FinalStateRevision
├── Records[]
└── FinalSnapshot : FPresentationStateSnapshot
```

Required guarantees:

```text
Records[] and FinalSnapshot describe the same exact sealed Resolution
Envelope contents never change after Seal
one (BattleId, ResolutionId) can Seal/Publish at most one Envelope
separate Resolutions cannot overwrite one another
historical rendering requires no mutable Gameplay lookup
empty-record Resolution is legal when recording remained valid
sealed Envelope lifetime is independent from active builder lifetime
```

The Controller never receives a ResolutionId and then queries the Recorder for “whatever records currently exist”.

`FinalStateRevision` identifies the stable state captured by the Resolution. It is not the Envelope's unique identity.

---

## 5. Recorder / RecordWriter dependency boundary

### 5.1 Recorder owns only the current builder

```text
Begin
→ Append
→ Append
→ Seal Envelope
→ clear builder immediately
```

Recorder is not a replay database and does not own presentation backlog or deferred-publication lifetime.

### 5.2 RecordWriter is optional, explicit and battle-scoped

Actions that can create player-facing committed facts receive/use a narrow optional battle-scoped RecordWriter/Sink dependency through explicit initialization/context propagation.

Hard rules:

```text
Action must not world-search for BattleManager/Recorder
Action must not use GetAllActorsOfClass
Action must not infer Recorder by casting UObject Outer
Action must not use a global/singleton Recorder
Gameplay Runtime owners must not depend on RecordWriter
```

Reaction and nested Actions created during one active Gameplay Resolution must inherit/receive the same active Resolution writer through their existing explicit action-building/dispatch context path.

The exact C++ interface may stay small in A2A, but the dependency must be explicit and optional.

### 5.3 Writer absence and append failure have different semantics

If Presentation recording is disabled before the Resolution begins, or there is intentionally no writer:

```text
Gameplay runs normally
→ no historical Records are expected
→ no historical Envelope is required for playback
→ stable boundary still freezes the latest FPresentationStateSnapshot baseline
→ presentation/no-controller path may apply that baseline directly
```

This is a valid no-history mode, not an error.

If a writer exists and an Append unexpectedly fails after one or more earlier Records may already have been accepted:

```text
mark current presentation builder invalid
↓
discard every buffered-but-unpublished Record for that Resolution
↓
do not Seal or Publish a partial historical Envelope
↓
continue Gameplay Commit / Action Finish / Queue ordering normally
↓
at stable boundary still Freeze the exact final presentation baseline
↓
enter PresentationUnavailable or equivalent fail-safe no-history catch-up for the battle
↓
never Gameplay ResolutionFault
```

Example that must never be published as a trustworthy historical Envelope:

```text
CardPlayed append succeeds
Damage append fails
Status append never occurs
FinalSnapshot already contains Damage + Status results
```

Publishing `[CardPlayed] + FinalSnapshot` as if it were complete history is forbidden. Once the active writer has failed, the Resolution's historical record batch is invalid as a whole.

Required regression:

```text
AppendFailureDoesNotSealPartialEnvelope
```

---

## 6. Freeze, Seal, identity and de-duplication policy

Read-state publication identity and Envelope identity are deliberately different:

```text
Envelope identity / de-duplication
= (BattleId, ResolutionId)

Read-state edge identity / de-duplication
= (BattleId, StateRevision)
```

Rules:

```text
one active ResolutionId → at most one Seal
one sealed (BattleId, ResolutionId) → at most one public Envelope delivery
same stable (BattleId, StateRevision) callback repeated
→ must not re-Seal the current/previous Resolution
→ must not duplicate OnReadStateReady for the same read edge
```

Do not use `LastPublishedReadStateRevision` or any equivalent read-edge cache as the sole Envelope identity. A historical operation batch and a stable state version have different semantics; future System/no-op/empty-record Resolutions must not be forced into a permanent one-to-one identity assumption.

A duplicate stable callback may be suppressed by the read-state de-duplication path, but Envelope de-duplication remains based on the Resolution identity that was actually sealed.

If raw read succeeds but Freeze fails, or Envelope construction/Seal fails:

```text
clear/discard current Presentation builder safely
mark Presentation unavailable/disabled for this battle
allow ordinary Gameplay state and deferred OnReadStateReady behavior to continue
never request Gameplay ResolutionFault
```

A Freeze/Seal failure must not leave a half-sealed builder that can later duplicate Records into another Envelope.

Required A2A regressions include:

```text
DuplicateStablePublishDoesNotDuplicateEnvelope
EnvelopeDedupUsesBattleIdAndResolutionId
FreezeFailureDisablesPresentationWithoutGameplayFault
SealFailureDoesNotReplayBuilderIntoNextResolution
```

---

## 7. Typed CommitResult / MutationResult boundary

Gameplay Runtime owns mutation truth but not Presentation context.

```text
Gameplay Runtime Commit
→ typed CommitResult / MutationResult

Action / BattleManager
→ owns Source / Reason / current Resolution writer
→ converts successful result into Presentation Record
```

### 7.1 Damage

```text
FDamageCommitResult
├── bCommitted
├── IncomingDamage
├── HPBefore
├── HPAfter
├── BlockBefore
├── BlockAfter
├── BlockedDamage
└── HPDamage
```

`ACombatant::TakeCombatDamage()` returns the result. `UDamageAction` combines it with Source/Target/DamageKind/resolved amount and writes the Damage Record.

The Damage Record is the single presentation fact for damage absorption and HP loss. It already owns `BlockBefore / BlockAfter / BlockedDamage` together with `HPBefore / HPAfter / HPDamage`. Damage consuming Block must not emit an additional `BlockChanged` Record for the same commit, otherwise playback would show the Block loss twice. Independent Block gain/clear operations still produce `BlockChanged` normally.

### 7.2 Block

`GainBlock()` / `ClearBlock()` return at least:

```text
FBlockCommitResult
├── bCommitted
├── BlockBefore
├── BlockAfter
└── ChangedAmount
```

`ClearBlock()` remains a direct BattleManager operation for UI-A2; do not migrate it to an Action only for Presentation.

```text
StartOpeningHand ClearBlock
→ initialization normalization
→ no visible Record

StartPlayerTurn / StartEnemyTurn ClearBlock
→ BlockChanged
→ Reason = TurnStartClear
```

### 7.3 Energy / CardPlayed

`CardPlayed` must preserve the energy commit historically. Do not read final/live Energy when playback occurs.

UI-A2C must implement either a dedicated `FEnergyCommitResult` or an equivalent exact PlayCard commit result. The player-facing CardPlayed Record must contain at least:

```text
CardRuntimeId
CardId
SourcePresentationId
TargetPresentationId when applicable
EnergyBefore
EnergyAfter
CostPaid
```

The intended visible order is:

```text
CardPlayed / Hand leaves current display
→ Energy Before → After
→ card effect Records such as Damage / Block / Draw
```

### 7.4 Deck/card zones

DeckRuntime returns a generic mutation fact rather than recording Presentation directly:

```text
FCardZoneMutationResult
├── bCommitted
├── CardRuntimeId
├── CardId
├── FromZone
└── ToZone
```

Cover:

```text
DrawPile → Hand
Hand → PlayArea
Hand → Discard
PlayArea → Discard
PlayArea → Exhaust
PlayArea → Removed
```

Action-level presentation maps these facts to `CardPlayed`, `CardDrawn`, `CardDiscarded`, `CardExhausted`, `CardRemoved` as needed.

Shuffle remains Action-level:

```text
Shuffle commit
→ Shuffle Presentation Record
→ FDeckShuffledEvent
→ reactions
→ RetryDraw
```

Initial setup shuffle emits no Shuffle Record.

Opening-Hand draws performed during `BattleStart` setup emit no `CardDrawn` Records even though they use the authoritative draw mutation path. Opening setup is normalization, not normal visible battle history:

```text
BattleStart
→ initial deterministic shuffle: no Record
→ opening-Hand draws: no Records
→ Seal empty-record BattleStart Envelope
→ apply its frozen FinalSnapshot directly
```

If opening-hand animation is desired later, add an explicit visible-opening policy switch and define its playback semantics. Do not obtain it by accidentally treating setup draws as ordinary `DrawCardAction` presentation.

### 7.5 Status

```text
FStatusMutationResult
├── bCommitted
├── StatusId
├── RuntimeSequence
├── AmountBefore
├── AmountAfter
├── bCreated
└── bRemoved
```

Use this shape for Apply/Reduce/RemoveStatusById. The Container reports only the mutation fact. Action/BattleManager adds Source and semantic reason such as `Applied`, `Increased`, `Reduced`, `TurnEndDecay` or `Removed`.

Direct test-only mutation does not auto-generate Presentation Records.

---

## 8. Frozen presentation state model

`FPresentationStateSnapshot` is the only complete display model used by both immediate baseline rendering and sealed Envelopes.

It freezes all values that the ViewModel currently needs so applying it never re-enters mutable Gameplay.

The first implementation field contract is:

```text
FPresentationStateSnapshot
├── BattleId
├── StateRevision
├── BattleState
├── Outcome
├── Energy / MaxEnergy
├── bCanEndTurn                         advisory display value for this revision
├── Player : FPresentationCombatantState
├── Enemy  : FPresentationCombatantState
├── HandCards[] : FPresentationCardState
├── DrawCount / DiscardCount / ExhaustCount
└── EnemyIntent : FPresentationIntentState

FPresentationCombatantState
├── resolved PresentationId
├── bPlayer
├── DisplayName
├── HP / MaxHP / Block / bDead
└── Statuses[] : FPresentationStatusState

FPresentationStatusState
├── StatusId / Amount
├── DisplayName
├── resolved dynamic Description
├── bUseAtlasIcon
├── UVOffset / UVScale
└── TrimOffset / TrimScale

FPresentationCardState
├── RuntimeId / CardId
├── DisplayName
├── Cost
├── CardType / TargetType
├── resolved dynamic Description
├── immutable CardArt reference
├── bGameplayPlayable
└── UnplayableReason

FPresentationIntentState
├── Type / DisplayName
├── committed BaseAmount
├── bHasCurrentResolvedDamageAmount
└── CurrentResolvedDamageAmount
```

This list freezes everything the current HUD derives from `FBattleReadSnapshot`, `CardData`, `StatusData`, current playability queries and the committed Intent read view. Applying it later must not access `UCardInstance`, `UCardData`, `UStatusInstance`, `UStatusData`, `ACombatant`, BattleManager Query APIs or current Enemy Intent.

Selection, hover, `LastFeedback`, legal-target runtime bindings and `PlaybackToken` remain transient presentation/input state and are not part of the historical snapshot. Existing self-contained HUD value structs may be reused or moved into this boundary; do not create a third parallel DTO hierarchy.

`RuntimeId` and resolved `PresentationId` remain value identities inside the frozen model. Any weak runtime objects needed to submit a new Request belong only to the latest caught-up input-binding cache described in Section 2.2.

---

## 9. Unified resolved PresentationId

Do not add a PresentationId Registry/validator UObject.

Use one Battle-layer resolver conceptually:

```cpp
bool ABattleManager::TryResolveCombatantPresentationId(
    const ACombatant* Combatant,
    FName& OutPresentationId
) const;
```

Rules:

```text
explicit authored PresentationId non-empty
→ use it

Combatant == Player
→ fallback Player

Combatant == current Enemy
→ fallback EnemyPrimary

not part of current battle
→ fail
```

Validate resolved IDs, not raw authored fields:

```text
all participants resolve
resolved ID non-empty
resolved IDs battle-scoped unique
```

Snapshot, LegalTargets and Presentation Records use the same resolved value. ViewModel fallback logic is removed once Battle-level resolution owns the semantic.

PresentationId is immutable for the battle lifetime. The implemented resolver locks each participant to the resolved IDs captured by the first exact frozen baseline, so later mutation of the authored `ACombatant::PresentationId` field cannot silently reroute later Snapshots, LegalTargets or Records within the same battle.

---

## 10. Presentation bootstrap failure

Invalid resolved PresentationId, Freeze bootstrap failure or other Presentation-only initialization failures must not become Gameplay `ResolutionFaulted`.

Use a UI-only state such as:

```text
PresentationUnavailable
```

Selected implementation policy for UI-A2A: **ViewModel initialization still succeeds into an explicit UI-only error state.**

```text
Presentation bootstrap validation fails
↓
ViewModel enters PresentationUnavailable
↓
normal player input remains disabled
↓
Presenter still creates the normal HUD Widget
↓
HUD shows a clear development-facing error panel/message
↓
Recorder/Controller may remain disabled for the battle
↓
headless Gameplay correctness is unchanged
```

Do not return early from Presenter before any Widget can show the failure.

---

## 11. Controller / Presenter ownership

`ABattleHUDPresenter` remains the assembly point. It may create/wire:

```text
BattleHUDViewModel
BattlePresentationController
Battle HUD Widget
```

The Presenter/Coordinator decides whether Presentation is enabled and owns sequencing between deferred Envelope delivery and ViewModel application.

`OnReadStateReady` is not a second HUD display owner.

### 11.1 Bounded Envelope queue

Controller owns the sealed backlog after public delivery, not Recorder.

```text
Envelope 12 → its Records → its FinalSnapshot
Envelope 13 → its Records → its FinalSnapshot
...
```

Each Envelope applies its own FinalSnapshot after completion/skip.

If backlog exceeds the configured UX bound:

```text
collapse/skip obsolete queued playback
→ Apply newest sealed Envelope.FinalSnapshot
→ discard obsolete transitional display state
→ if newest BattleId/Revision is caught up, refresh live input bindings
```

Do not re-pull display state from Gameplay during collapse.

Keep the first implementation minimal. Because normal player input remains locked until catch-up, ordinary player-origin flow normally has at most one Envelope awaiting visible completion. Multiple queued Envelopes mainly protect Debug/API/System calls and future autonomous producers. Use only:

```text
fixed queue bound
+ FIFO ordering
+ deterministic overflow collapse to the newest retained FinalSnapshot
```

Do not add ACK protocols, persistence, priority scheduling, per-Origin queue policy, producer backpressure, adaptive batching or a general presentation scheduler until a real producer requires it. The same minimal-policy rule applies to the pre-public-delivery FIFO.

### 11.2 PlaybackToken belongs to UI-A2A

The generic playback completion safety protocol is infrastructure and must exist before UI-A2B adds real Damage/Block playback:

```text
PlayPresentationRecord(Record, PlaybackToken)
↓
NotifyPresentationFinished(PlaybackToken)
```

Controller must:

```text
ignore duplicate completion
ignore stale Resolution/Sequence callback
ignore previous-Battle callback
ignore callback from before Skip/generation reset
ignore timeout callbacks whose scheduled token no longer matches the active token
support Skip / fast-forward
catch up when Widget is destroyed
ignore stale destruction from an already-replaced Widget
use timeout/immediate fallback when Blueprint never completes
apply FinalSnapshot when Presentation is disabled
```

Timeout/Skip/fallback advances or collapses Presentation only. It never advances Gameplay or requests Gameplay fault.

---

## 12. Slice ownership — no cross-phase ambiguity

### UI-A2A — infrastructure — C++ VALIDATED / 8/8

Implemented source scope:

```text
generic Record/Envelope transport
BattleId / ResolutionId / PresentationSequence
Resolution Origin / Begin / Abort / internal Seal lifecycle
seal-before-next-Begin invariant
ResolutionFault Record lifecycle and append-last invariant
frozen FPresentationStateSnapshot
RecordWriter explicit optional propagation
append-failure invalidation / no partial Envelope
immutable Envelope pairing
separate Envelope vs read-state de-duplication
Freeze/Seal failure policy
public deferred Envelope/Ready notification
no accepted-Request callback reentrancy
PresentationUnavailable bootstrap path
Presenter/Controller display ownership
bounded Envelope backlog
PlaybackToken
Skip / missing callback / stale callback / timeout / Widget-loss fail-safe
latest-only runtime input binding refresh
```

A2A does **not** implement real Damage/Block business records or animation. Its infrastructure is validated; visible presentation remains part of the deferred unified Blueprint integration.

### UI-A2B — Damage + Block vertical slice — C++ VALIDATED / 8/8

Adds:

```text
FDamageCommitResult
FBlockCommitResult
Damage Record
BlockChanged Record
fully blocked damage
damage-consumed Block is represented inside Damage Record only; no duplicate BlockChanged Record
TurnStartClear Block record
lethal Damage → Victory/Defeat Record ordering
simple Damage/Block playback
```

The generic PlaybackToken/Skip/fail-safe already exists from A2A.

Victory/Defeat ordering is exercised here because lethal Damage is the first concrete terminal-producing presentation path.

### UI-A2C — cards/deck/energy — C++ VALIDATED / 8/8

Adds:

```text
CardPlayed
exact EnergyBefore / EnergyAfter / CostPaid history
CardZoneChanged
Draw
Hand discard
PlayArea → Discard
PlayArea → Exhaust
PlayArea → Removed
Shuffle → reactions → RetryDraw ordering
```

### UI-A2D — Status + formal terminal/fault visual presentation — DESIGN LOCKED / NOT IMPLEMENTED

Adds:

```text
Status create / merge / reduce / remove
Status change reason
formal Victory / Defeat visual treatment
formal ResolutionFault visual treatment
combined end-to-end presentation acceptance
```

`ResolutionFault` lifecycle itself already exists from A2A; A2D adds the normal visible treatment and combined acceptance rather than introducing the record type for the first time.

The complete A2D contract, including exact Status MutationResult/identity, historical descriptions, terminal reducer validation and producer matrix, is `docs/Phase6UIA2DImplementation.md`.

---

## 13. Internal sealing vs deferred public publication

### 13.1 Internal Gameplay-stable sealing boundary

When the Queue/macro flow becomes stable, sealing is synchronous Gameplay-side bookkeeping and must finish before another Resolution can begin:

```text
Queue/macro flow stable
↓
Build raw player-facing read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
if recording builder is valid:
    Seal (BattleId, ResolutionId) exactly once
    move immutable Envelope into pending-publication storage
else if recording was intentionally disabled:
    keep only the frozen latest baseline
else if append/freeze/seal failed:
    discard invalid builder/history
    keep frozen baseline when available
    mark Presentation unavailable/degraded
↓
clear/release active builder
```

Do not wait for the CoreTicker/public Ready edge to release the builder.

### 13.2 Deferred public-notification boundary

The public delivery step remains deferred so accepted Requests cannot be completed re-entrantly from inside their own call stack:

```text
originating public Request has returned
↓
public deferred callback/ticker boundary
↓
if sealed Envelopes are pending and Presentation delivery is enabled:
    drain PendingPublicDeliveryQueue in ResolutionId order
    publish OnPresentationResolutionReady(Envelope) for each entry
↓
update/expose current latest frozen baseline as appropriate
↓
OnReadStateReady(BattleId, StateRevision)
```

`OnPresentationResolutionReady` and `OnReadStateReady` obey the same non-reentrancy rule for accepted public Requests.

No active Resolution exists merely because a sealed Envelope is still waiting for public delivery.

The public delivery FIFO and Controller playback queue are separate bounded queues:

```text
Recorder active builder
→ Seal
→ Battle-side PendingPublicDeliveryQueue
→ deferred public broadcast
→ Controller playback backlog
```

The first prevents loss before notification; the second manages visual playback after notification. Neither is a long-term history store. A single pending slot is forbidden because another Resolution may Seal before the deferred callback executes.

### 13.3 De-duplication ownership

```text
Envelope Seal/Publish de-dup
→ (BattleId, ResolutionId)

ordinary stable read-edge de-dup
→ (BattleId, StateRevision)
```

A duplicate stable-read callback must not cause another Envelope Seal. Conversely, Envelope identity must not be inferred solely from the last published read revision.

`OnReadStateReady` may coalesce current-state observation to the newest revision according to its existing scheduler, but sealed Presentation Envelopes may not be lost or overwritten by that coalescing. Pending delivery uses `(BattleId, ResolutionId)` ordering and identity.

When no active Resolution exists, a new stable baseline may still be frozen for late HUD initialization without inventing a fake historical Resolution.

---

## 14. UI-A2A Automation gate

Before UI-A2B visible Damage/Block playback begins, focused tests must cover at least:

```text
AcceptedRequestEstablishesResolutionBeforeExecution
OrdinaryValidationRejectionCreatesNoResolution
PostValidationFrameworkFaultSealsFaultResolution
BattleStartBeginsBeforeFaultCapableOpeningWork
BattleStartOpeningDrawCreatesNoPresentationRecords
SystemResolutionCanBeCreated
EmptyRecordResolutionSealsSafely
FaultRetainsCommittedRecordsAndAppendsResolutionFaultLast
ResolutionSealsBeforeNextRequestCanBegin
PresentationEnvelopeNotificationDoesNotReenterAcceptedRequest
OneActiveResolutionSealsAtMostOnce
DuplicateStablePublishDoesNotDuplicateEnvelope
EnvelopeDedupUsesBattleIdAndResolutionId
MultipleSealedBeforeDeferredDeliveryPreserveResolutionOrder
BattleRestartClearsPendingPublicDeliveryQueue
PendingDeliveryOverflowFallsBackWithoutGameplayFault
FreezeFailureDisablesPresentationWithoutGameplayFault
SealFailureDoesNotLeakBuilderIntoNextResolution
AppendFailureDoesNotSealPartialEnvelope
BattleRestartDoesNotLeakBuilderOrRecords
LateSubscriberDoesNotReplayOldBattle
NoControllerOrPresentationDisabledLeavesGameplayUnchanged
HistoricalFrozenSnapshotAppliesWithoutMutableRuntimeReads
FrozenSnapshotContainsCompleteCurrentHUDDisplayValues
HistoricalEnvelopeCannotUseLiveInputBindings
InputBindingsRefreshOnlyAtNewestMatchingBattleRevision
OnReadStateReadyCannotBypassActivePresentationSequencing
RecordWriterIsOptionalAndExplicit
NestedReactionUsesSameActiveResolutionWriter
RecordAppendFailureDoesNotChangeGameplayFinishOrQueue
ControllerBacklogIsBounded
PlaybackTokenDuplicateAndStaleCompletionIgnored
SkipMissingCallbackTimeoutWidgetLossCatchUpWithoutGameplayFault
ResolvedPresentationIdSharedBySnapshotTargetsAndRecords
InvalidResolvedPresentationIdShowsPresentationUnavailable
PresentationUnavailableStillCreatesErrorCapableHUD
```

These semantics are authored across:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA2AInfrastructureTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2APresenterTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2AHardeningTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2ATestTypes.h/.cpp
```

The hardening suite additionally covers overlapping-Begin builder cleanup, terminal fault append rejection, battle-lifetime resolved PresentationId stability, stale old-battle Envelope rejection, stale old-Widget loss isolation and direct frozen-baseline HUD operation when committed recording is disabled.

These A2A suites passed 8/8 in the owner-confirmed UE5.8 affected aggregate together with A2B 8/8 and A2C 8/8 (77/77 total). This evidence does not include deferred Blueprint playback or PIE presentation smoke.

---

## 15. USER ACTION REQUIRED boundary

UI-A2A should be Automation-first and should not require new UMG animation work.

When UI-A2B begins visible playback, user-side UE Editor work may be required to wire Blueprint presentation callbacks/animations. At that time instructions must specify the exact Widget asset, event/function, property values, expected sequence and validation result.

---

## 16. Acceptance principle

UI-A2 is successful when presentation is a deterministic, bounded, fail-safe explanation of facts that Gameplay already committed.

The final invariant is:

```text
Gameplay result
= independent of Presentation speed / availability / callback correctness

Historical display
= driven only by sealed Records + that Envelope.FinalSnapshot

Resolution lifecycle
= internal Seal releases the builder before any next Begin; public delivery may be deferred

Envelope identity
= BattleId + ResolutionId

Read-state edge identity
= BattleId + StateRevision

Current input identity
= refreshed only after display catches up to newest matching BattleId/Revision
```

UI-A2A infrastructure and the A2B/A2C committed-fact reducers are C++/Automation validated. Visible Blueprint integration remains intentionally deferred until A2D C++/Automation is closed, after which Damage/Block/Card/Energy/Zone/Status/Terminal playback should be wired and PIE-smoked as one coherent presentation surface.
