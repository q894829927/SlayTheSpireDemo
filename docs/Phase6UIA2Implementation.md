# Phase 6UI-A2 — Basic Committed Presentation

Status: **DESIGN LOCKED / UI-A2A IMPLEMENTATION NEXT**.

UI-A2 replaces the UI-A0/UI-A1 immediate/no-op presentation catch-up boundary with deterministic playback of already-committed gameplay facts. It does not make `BattleActionQueue`, `BattleState` or authoritative gameplay wait for animation.

This document is the detailed implementation contract. UI-A2A establishes transport, resolution, freezing, failure and playback-safety infrastructure before any real Damage/Block animation is added.

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
Build exact raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal immutable FPresentationResolutionEnvelope exactly once
↓
Presentation Coordinator / Controller
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

### 2.3 `OnReadStateReady` cannot bypass Presentation

`OnReadStateReady(BattleId, StateRevision)` remains a non-replaying edge meaning that a stable current state can be read. It is not a historical payload.

Stable publication order once UI-A2 is enabled:

```text
Build exact raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal active Envelope exactly once
↓
hand sealed Envelope to Presenter/Coordinator
↓
then notify ordinary OnReadStateReady observers
```

Display ownership is singular:

```text
Presentation enabled
→ Presenter/Controller drives ViewModel display using frozen snapshots
→ ViewModel must not Pull+Apply live state from OnReadStateReady

Presentation disabled / no Controller
→ apply the same newest frozen baseline immediately
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

### 3.4 Fault lifecycle belongs to UI-A2A

`ResolutionFault` Record lifecycle is infrastructure, not an A2D-only feature.

If a fault occurs after the builder exists:

```text
already committed Records remain
→ append ResolutionFault as the final Record
→ freeze ResolutionFaulted FinalSnapshot
→ Seal once
```

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
Envelope contents never change after publication
one active Resolution seals at most once
separate Resolutions cannot overwrite one another
historical rendering requires no mutable Gameplay lookup
empty-record Resolution is legal
```

The Controller never receives a ResolutionId and then queries the Recorder for “whatever records currently exist”.

---

## 5. Recorder / RecordWriter dependency boundary

### 5.1 Recorder owns only the current builder

```text
Begin
→ Append
→ Append
→ Seal Envelope
→ clear builder
```

Recorder is not a replay database and does not own presentation backlog.

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

### 5.3 Append failure is Presentation-only failure

If the writer is absent, disabled, invalid or rejects an append:

```text
Gameplay Commit remains committed
Action still follows its normal Finish contract
BattleActionQueue ordering is unchanged
no Gameplay ResolutionFault is requested
Presentation for the battle is disabled/degraded safely
```

Do not make Presentation recording a prerequisite for Gameplay correctness.

---

## 6. Freeze, Seal and duplicate-stable-publication failure policy

Current stable-state publication already de-duplicates `(BattleId, StateRevision)`. UI-A2 must preserve that property.

Rules:

```text
one active Resolution → at most one Seal
same stable BattleId/StateRevision published again
→ no duplicate Envelope Seal
→ no duplicate Envelope broadcast
```

Seal must occur only for a stable publication that is actually accepted as new for the relevant battle/read boundary.

If raw read succeeds but Freeze fails, or Envelope construction/Seal fails:

```text
clear/discard current Presentation builder safely
mark Presentation unavailable/disabled for this battle
allow ordinary Gameplay state and OnReadStateReady behavior to continue
never request Gameplay ResolutionFault
```

A Freeze/Seal failure must not leave a half-sealed builder that can later duplicate Records into another Envelope.

Required A2A regressions include:

```text
DuplicateStablePublishDoesNotDuplicateEnvelope
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

Conceptually include:

```text
BattleId / StateRevision / BattleState / Outcome
Energy / MaxEnergy
Player/Enemy HP / MaxHP / Block / DisplayName / resolved PresentationId
frozen Status display values: StatusId / Amount / DisplayName / Description / icon metadata
frozen Hand/Card display values: RuntimeId / CardId / DisplayName / Cost / CardType / TargetType / Description / CardArt
frozen pile counts/views required by current HUD
committed Enemy Intent + current-state resolved player-facing value
frozen advisory playability/reason for the displayed latest state when appropriate
```

Do not create a third parallel HUD DTO hierarchy. The ViewModel should become an adapter/state holder over this frozen model rather than recomputing display semantics from runtime objects.

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

PresentationId is treated as immutable for the battle lifetime.

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

The Presenter/Coordinator decides whether Presentation is enabled and owns sequencing between Envelope delivery and ViewModel application.

`OnReadStateReady` is not a second HUD display owner.

### 11.1 Bounded Envelope queue

Controller owns the sealed backlog, not Recorder.

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
support Skip / fast-forward
catch up when Widget is destroyed
use timeout/immediate fallback when Blueprint never completes
apply FinalSnapshot when Presentation is disabled
```

Timeout/Skip/fallback advances or collapses Presentation only. It never advances Gameplay or requests Gameplay fault.

---

## 12. Slice ownership — no cross-phase ambiguity

### UI-A2A — infrastructure

Must implement and test:

```text
generic Record/Envelope transport
BattleId / ResolutionId / PresentationSequence
Resolution Origin / Begin / Abort / Seal lifecycle
ResolutionFault Record lifecycle and append-last invariant
frozen FPresentationStateSnapshot
RecordWriter explicit optional propagation
immutable Envelope pairing
Freeze/Seal failure policy and duplicate stable-publication de-duplication
PresentationUnavailable bootstrap path
Presenter/Controller display ownership
bounded Envelope backlog
PlaybackToken
Skip / missing callback / stale callback / timeout / Widget-loss fail-safe
latest-only runtime input binding refresh
```

A2A does **not** require real Damage/Block animation.

### UI-A2B — Damage + Block vertical slice

Adds:

```text
FDamageCommitResult
FBlockCommitResult
Damage Record
BlockChanged Record
fully blocked damage
TurnStartClear Block record
lethal Damage → Victory/Defeat Record ordering
simple Damage/Block playback
```

The generic PlaybackToken/Skip/fail-safe already exists from A2A.

Victory/Defeat ordering is exercised here because lethal Damage is the first concrete terminal-producing presentation path.

### UI-A2C — cards/deck/energy

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

### UI-A2D — Status + formal terminal/fault visual presentation

Adds:

```text
Status create / merge / reduce / remove
Status change reason
formal Victory / Defeat visual treatment
formal ResolutionFault visual treatment
combined end-to-end presentation acceptance
```

`ResolutionFault` lifecycle itself already exists from A2A; A2D adds the normal visible treatment and combined acceptance rather than introducing the record type for the first time.

---

## 13. Stable publication, Envelope and `OnReadStateReady`

At the stable boundary:

```text
Queue/macro flow stable
↓
Build raw player-facing read snapshot
↓
reject duplicate stable publication key when already published/sealed
↓
Freeze exact presentation snapshot
↓
Seal active Resolution once
↓
Publish/hand Envelope to Presentation Coordinator when available
↓
update current latest frozen baseline
↓
OnReadStateReady(BattleId, StateRevision)
```

Freeze/Seal failure does not suppress Gameplay state publication. It disables/degrades Presentation for the battle and ordinary stable-read notification continues.

When no active Resolution exists, a new stable baseline may still be frozen for late HUD initialization without inventing a fake historical Resolution.

---

## 14. UI-A2A Automation gate

Before UI-A2B visible Damage/Block playback begins, focused tests must cover at least:

```text
AcceptedRequestEstablishesResolutionBeforeExecution
OrdinaryValidationRejectionCreatesNoResolution
PostValidationFrameworkFaultSealsFaultResolution
BattleStartBeginsBeforeFaultCapableOpeningWork
SystemResolutionCanBeCreated
EmptyRecordResolutionSealsSafely
FaultRetainsCommittedRecordsAndAppendsResolutionFaultLast
OneActiveResolutionSealsAtMostOnce
DuplicateStablePublishDoesNotDuplicateEnvelope
FreezeFailureDisablesPresentationWithoutGameplayFault
SealFailureDoesNotLeakBuilderIntoNextResolution
BattleRestartDoesNotLeakBuilderOrRecords
LateSubscriberDoesNotReplayOldBattle
NoControllerOrPresentationDisabledLeavesGameplayUnchanged
HistoricalFrozenSnapshotAppliesWithoutMutableRuntimeReads
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

Only after this gate is green should UI-A2B begin real Damage/Block presentation.

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

Current input identity
= refreshed only after display catches up to newest matching BattleId/Revision
```

No Damage animation should be implemented until UI-A2A satisfies these infrastructure contracts.