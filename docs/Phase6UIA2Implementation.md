# Phase 6UI-A2 — Basic Committed Presentation

Status: **DESIGN LOCKED / UI-A2A IMPLEMENTATION NEXT**.

UI-A2 replaces the UI-A0/UI-A1 immediate/no-op presentation catch-up boundary with deterministic playback of already-committed gameplay facts. It does not make `BattleActionQueue`, `BattleState` or authoritative gameplay wait for animation.

The first implementation goal is architecture correctness and gameplay legibility, not final animation quality.

## 1. Core model

```text
Request / System operation
↓
Begin Resolution
↓
BattleActionQueue
↓
Gameplay Commit
↓
typed CommitResult / MutationResult
↓
Action or BattleManager appends Presentation Record
↓
Gameplay continues deterministically
↓
macro flow becomes fully stable
↓
Build exact raw read snapshot
↓
Freeze exact presentation snapshot
↓
Seal immutable Resolution Envelope
↓
Presentation Coordinator / Controller
↓
bounded Envelope queue
↓
play Records in deterministic order
↓
Finished / Skip / fail-safe
↓
Apply Envelope.FinalSnapshot
↓
refresh latest runtime input bindings only when caught up
↓
unlock input when gameplay is request-eligible
```

Durable separation:

```text
Gameplay
= what is true

Frozen presentation snapshot
= what the player-facing state was at one exact stable revision

Presentation Record
= one meaningful already-committed historical fact

PresentationController
= when/how those facts are shown
```

`BattleActionQueue != PresentationQueue`.

Presentation failure, delay, skip or disablement must never alter authoritative gameplay results.

---

## 2. Four UI-A2A hard rules

These rules are mandatory before Damage animation or other visible A2 playback work begins.

### 2.1 Frozen snapshot is the only historical display input

During presentation playback, historical UI state must come only from a frozen presentation snapshot stored in the sealed Envelope.

Do not render an old revision by re-reading mutable runtime objects.

Current `FBattleReadSnapshot` is a gameplay/read-boundary structure and may contain weak runtime references. UI-A2 therefore introduces one frozen player-facing model:

```text
FBattleReadSnapshot
= current coherent gameplay read state
= may contain weak runtime references

FPresentationStateSnapshot
= complete frozen player-facing values
= Envelope and immediate HUD baseline share this model

BattleHUDViewModel
= copies/applies FPresentationStateSnapshot
```

`FPresentationStateSnapshot` may hold immutable presentation assets such as `UTexture2D*`, but it must not require mutable gameplay runtime references such as:

```text
UCardInstance
UStatusInstance
ACombatant
ABattleManager
```

Applying a frozen snapshot must not call back into gameplay to obtain historical card definitions, current status state, target legality or card playability.

### 2.2 Runtime input bindings exist only at the latest caught-up revision

The formal gameplay request boundary currently still requires runtime objects:

```text
RequestPlayCard(UCardInstance*, ACombatant*)
```

Therefore UI-A2 keeps a separate non-authoritative live interaction binding cache for the newest stable revision only.

Conceptually:

```text
Frozen Presentation Snapshot
= display only

Live Interaction Binding
= RuntimeId → TWeakObjectPtr<UCardInstance>
= TargetId → TWeakObjectPtr<ACombatant>
= only valid for the newest caught-up BattleId / StateRevision
```

Rules:

```text
historical Envelope playback
→ never uses runtime input bindings
→ normal input remains locked

Controller catches up to newest Envelope
→ apply newest frozen snapshot
→ refresh input bindings through the current formal read/gameplay boundary
→ verify BattleId / StateRevision match the displayed newest state
→ only then allow normal input
```

The binding cache is request-forwarding infrastructure, not gameplay authority. Formal Requests still revalidate current authoritative gameplay state.

Do not expand UI-A2A into `RequestPlayCardByRuntimeId()` unless a later concrete need justifies that API change.

### 2.3 `OnReadStateReady` must not bypass presentation playback

`OnReadStateReady(BattleId, StateRevision)` remains a stable-read edge notification. It must not remain a second path that directly applies final live state while a `PresentationController` is playing historical Envelopes.

When committed presentation is enabled:

```text
stable gameplay boundary
↓
Build raw read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal active Envelope
↓
hand Envelope to the presentation coordinator/controller
↓
then notify ordinary OnReadStateReady observers
```

Display ownership must be singular:

```text
Presentation enabled
→ Controller/Presenter drives ViewModel display through frozen snapshots
→ ViewModel does not directly Pull+Apply on OnReadStateReady

Presentation disabled / no Controller
→ use the current frozen baseline immediately
→ no historical playback required
```

`OnReadStateReady` means only that a new stable state is available to read. It does not promise that a later pull can reconstruct the exact historical revision named by an already-finished notification.

The Envelope is the exact history/snapshot pairing contract.

### 2.4 Ordinary validation rejection creates no Resolution; post-validation framework fault does

Normal authoritative validation failure is not a gameplay resolution:

```text
Invalid card / target / energy / turn / battle state
→ Request rejected
→ no BeginResolution
→ no Presentation Envelope
```

But a request may pass gameplay validation and then encounter framework failure while preparing or inserting required work. Such a path may return `Rejected` while also requesting/entering `ResolutionFaulted`.

That fault must still have a presentation Resolution.

Preferred lifecycle:

```text
final gameplay validation passes
↓
BeginResolution(Origin)
↓
prepare / atomically enqueue required work
↓
success
→ continue normal resolution

framework fault
→ preserve any already-committed Records
→ append ResolutionFault Record
→ capture frozen fault FinalSnapshot
→ Seal fault Envelope

ordinary no-side-effect preparation failure that is not a framework fault
→ Abort builder
```

`BattleStart` begins its Resolution before any opening-hand/setup operation that can produce a framework fault.

---

## 3. Resolution lifecycle

### 3.1 Identity

Each committed presentation record belongs to:

```text
BattleId
ResolutionId
PresentationSequence
```

Requirements:

```text
BattleId
→ prevents presentation leakage across battles

ResolutionId
→ groups one player-visible/system resolution

PresentationSequence
→ deterministic total order of Records inside the battle
```

`PresentationSequence` is independent from `StateRevision`.

Do not use:

```text
Widget creation order
delegate registration order
frame timing
animation start time
UObject address
unordered container iteration
```

as presentation ordering.

### 3.2 Origins

First implementation uses only the concrete origins currently needed:

```text
BattleStart
PlayCard
EndTurn
System
```

Do not predeclare speculative AI/Relic/Replay origins merely for future completeness.

### 3.3 Player Requests

A normal accepted player request establishes its presentation Resolution before any queued Action can execute.

```text
RequestPlayCard
→ final validation passes
→ BeginResolution(PlayCard)
→ prepare / enqueue
→ StartProcessing
```

```text
RequestEndPlayerTurn
→ final validation passes
→ BeginResolution(EndTurn)
→ prepare / enqueue turn-ending batch
→ commit PlayerTurnEnding
→ StartProcessing
```

The current complete End Turn macro flow belongs to one Resolution:

```text
remaining Hand cleanup
→ Player TurnEnded reactions
→ Enemy turn
→ Enemy TurnEnded reactions
→ next PlayerTurnStarting work
→ next player Draw work
→ stable PlayerTurn
```

Do not split this macro flow into multiple presentation Resolutions unless a later concrete UX/architecture need requires it.

### 3.4 BattleStart / System

Opening Hand is not a formal UI Request, so it uses `BattleStart` origin.

Other non-player-Request work that needs a Resolution may use `System` until a concrete mechanic requires a more specific origin.

### 3.5 Faults

If a fault occurs after a builder exists:

```text
already committed Records remain
→ append ResolutionFault
→ freeze ResolutionFaulted FinalSnapshot
→ Seal
```

If a framework fault occurs after validation but before the normal Resolution could otherwise proceed, the builder still exists and produces the fault Envelope.

Do not discard committed presentation history because the same resolution later faults.

---

## 4. Immutable Resolution Envelope

The Controller must not ask the Recorder, BattleManager or current runtime to reconstruct which Records belonged to a completed resolution.

Gameplay seals one immutable batch:

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
Records[] and FinalSnapshot describe the same sealed resolution
Envelope contents do not change after publication
separate resolutions cannot overwrite one another
historical rendering requires no mutable gameplay lookup
```

An empty-record Resolution is legal. It still may carry a meaningful final stable snapshot.

The exact snapshot must be captured at the stable sealing boundary, not later by the Controller.

---

## 5. Recorder responsibilities

The Recorder is a current-resolution builder, not a replay database and not the presentation backlog owner.

```text
BeginResolution
↓
Append Record
↓
Append Record
↓
...
↓
SealResolution(FinalSnapshot)
↓
produce immutable Envelope
↓
clear current builder
```

The Recorder must not be required for Gameplay correctness.

```text
no presentation consumer
presentation disabled
HUD unavailable
```

must all leave gameplay results unchanged.

Do not make Recorder overflow or presentation backlog pressure produce `ResolutionFaulted`.

---

## 6. CommitResult / MutationResult boundary

Gameplay runtime objects report authoritative mutation facts. They do not depend on Presentation Recorder, ResolutionId or UI concepts.

Pattern:

```text
Gameplay Runtime Commit
→ typed CommitResult / MutationResult

Action / BattleManager
→ owns Source / Reason / operation context / current Resolution
→ converts the result into a player-facing Presentation Record
```

This boundary is required so Presentation may be fully disabled without changing Gameplay.

### 6.1 Damage

Conceptual result:

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

`ACombatant::TakeCombatDamage()` returns the commit result.

`UDamageAction` combines that result with its existing operation context:

```text
Source
Target
DamageKind
resolved amount
```

and appends the Damage Presentation Record.

Do not make `ACombatant` depend on Presentation merely to obtain Source/Reason/Resolution metadata.

### 6.2 Block

`GainBlock()` / `ClearBlock()` expose a typed block commit result containing at least:

```text
bCommitted
BlockBefore
BlockAfter
ChangedAmount
```

`UGainBlockAction` records ordinary gain context.

Current `ClearBlock()` is directly called from BattleManager at turn boundaries. UI-A2 does not migrate those calls into Actions merely for presentation.

Semantics:

```text
StartOpeningHand ClearBlock
→ battle initialization normalization
→ no player-visible BlockChanged Record

StartPlayerTurn ClearBlock
→ record BlockChanged
→ Reason = TurnStartClear

StartEnemyTurn ClearBlock
→ record BlockChanged
→ Reason = TurnStartClear
```

If future gameplay requires Block clear to be modified/intercepted/reacted to, move it into a proper gameplay operation at that time rather than as an incidental UI-A2 refactor.

### 6.3 Deck / card zones

DeckRuntime returns typed zone mutation facts; it does not record presentation directly.

Prefer a low-level fact such as:

```text
FCardZoneMutationResult
├── bCommitted
├── CardRuntimeId
├── CardId
├── FromZone
└── ToZone
```

It must cover both normal Hand cleanup and resolved card destinations:

```text
DrawPile → Hand
Hand → PlayArea
Hand → Discard
PlayArea → Discard
PlayArea → Exhaust
PlayArea → Removed
```

Action-level presentation may map these facts to player-facing operations such as:

```text
CardPlayed
CardDrawn
CardDiscarded
CardExhausted
CardRemoved
```

`CardPlayed` is part of UI-A2 because it establishes the visual order for Hand removal / Energy spending before card effects such as Damage.

Shuffle is special: `UShuffleDeckAction` records `Shuffle` only after a successful DeckRuntime shuffle commit, before dispatching `FDeckShuffledEvent` reactions.

Required order remains:

```text
Shuffle commit
→ Shuffle Presentation Record
→ FDeckShuffledEvent
→ reactions
→ RetryDraw
```

Initial battle setup shuffle emits neither `DeckShuffled` nor a Shuffle Presentation Record.

### 6.4 Status

`UStatusContainer` returns a typed mutation result rather than depending on Presentation:

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

Use the same result model for:

```text
ApplyStatus
ReduceStatus
RemoveStatusById
```

`RemoveStatusById` currently has no normal gameplay path, but standardizing its result now is small and prevents a future silent presentation gap. Direct test calls still do not generate Records automatically.

`StatusContainer` describes only the mutation fact. It must not decide the semantic reason.

`UApplyStatusAction` / `UReduceStatusAction` / future removal caller adds context such as:

```text
SourcePresentationId
ChangeReason
```

Initial reason vocabulary may include only real implemented needs:

```text
Applied
Increased
Reduced
TurnEndDecay
Removed
```

Do not infer historical reason later by diffing status arrays.

---

## 7. PresentationId authority

Do not add a separate PresentationId registry/validator subsystem for UI-A2.

Move the current fallback semantics into one Battle-layer resolver used by:

```text
raw read snapshot
legal-target presentation mapping
Presentation Records
```

Preferred shape:

```cpp
bool ABattleManager::TryResolveCombatantPresentationId(
    const ACombatant* Combatant,
    FName& OutPresentationId
) const;
```

The caller does not pass `bPlayer`.

Current single-enemy fallback rules:

```text
explicit Combatant.PresentationId is non-empty
→ use explicit value

Combatant == current Player
→ fallback Player

Combatant == current Enemy
→ fallback EnemyPrimary

combatant is not part of this battle
→ fail
```

Validate **resolved** IDs before the first presentation snapshot/record is used:

```text
all current combatants resolve successfully
resolved IDs are non-empty
resolved IDs are unique inside the battle
```

Do not require raw `PresentationId` fields to be non-empty because valid fallback configuration is supported.

The resolved ID is battle-scoped and must be treated as immutable during that battle.

Future multiple-enemy work must not derive IDs from UObject address, Actor enumeration order or localized display text.

### Presentation bootstrap failure

Invalid presentation identity is not a Gameplay `ResolutionFaulted` condition.

Instead:

```text
Gameplay
→ may continue correctly headless

Presentation bootstrap
→ fails visibly
→ Recorder/presentation routing disabled for that battle
→ normal player HUD input disabled
→ explicit development error surface shown
```

Use a presentation-layer state such as `PresentationUnavailable`; do not add it to `EBattleState`.

A packaged/dev build must not appear silently frozen merely because presentation configuration is invalid.

---

## 8. Stable sealing boundary and event ownership

`OnReadStateReady` remains a stable-read edge, but UI-A2 must prevent it from bypassing presentation.

The final stable boundary should conceptually perform:

```text
Build raw coherent read snapshot
↓
resolve/finalize player-facing values
↓
Freeze FPresentationStateSnapshot
↓
if active Resolution exists:
    append terminal/fault record when required
    Seal FPresentationResolutionEnvelope
    publish OnPresentationResolutionReady(Envelope)
↓
publish ordinary OnReadStateReady(BattleId, StateRevision)
```

The Envelope publication must happen from the same exact stable state used to produce its `FinalSnapshot`.

Do not implement:

```text
OnReadStateReady
→ Controller guesses ResolutionId
→ Controller asks Recorder for current records
```

### Initialization / late subscription

`OnPresentationResolutionReady` is not a history replay database.

A controller created after old Envelopes have already published:

```text
does not replay a previous Battle or previous Resolution
↓
pulls/builds the current frozen presentation baseline
↓
establishes current display
↓
refreshes matching current input bindings if request-eligible
↓
consumes only future Envelopes
```

This preserves the existing durable subscribe-then-pull principle without pretending historical events are state containers.

---

## 9. PresentationController and bounded backlog

The Controller owns presentation backlog. The Recorder does not.

Conceptually:

```text
Envelope 12
Records 101..104
FinalSnapshot Revision 30

Envelope 13
Records 105..108
FinalSnapshot Revision 31
```

The Controller applies each Envelope's own `FinalSnapshot` when that Envelope finishes.

Do not use one overwriteable `PendingSnapshot`.

### Bounded storage

The Envelope queue must be explicitly bounded by presentation policy.

If backlog exceeds the supported UX limit:

```text
cancel/collapse obsolete playback
↓
consume or discard intermediate presentation-only backlog safely
↓
Apply newest available FinalSnapshot
↓
refresh newest matching runtime input bindings
↓
resume from caught-up state
```

Backlog collapse must never alter gameplay and must never request Gameplay `ResolutionFaulted`.

Do not allow unbounded presentation storage.

---

## 10. Playback token / fail-safe contract

Blueprint animation completion is not trusted as an always-correct gameplay-critical signal.

Playback shape:

```text
PlayPresentationRecord(Record, PlaybackToken)
↓
Blueprint/C++ visual work
↓
NotifyPresentationFinished(PlaybackToken)
```

The token must identify the current presentation playback strongly enough to reject stale/duplicate callbacks, conceptually including:

```text
BattleId
ResolutionId
PresentationSequence
local PlaybackGeneration
```

Controller must handle:

```text
duplicate Finished
late callback from an older Record
late callback from an older Battle
callback after Skip
Widget destroyed during playback
Blueprint event not implemented
presentation disabled
explicit Skip / Fast-forward
presentation timeout / fail-safe
```

Rules:

```text
invalid/stale token
→ ignore

duplicate completion
→ ignore

Blueprint missing / widget destroyed / timeout
→ skip or immediate-complete presentation only
→ never mutate Gameplay
→ never request Gameplay ResolutionFault
```

If presentation is disabled, the Controller immediately applies the Envelope's frozen final snapshot.

---

## 11. Terminal ordering

Terminal/fault records are historical presentation facts appended after the commit that caused the terminal state.

Victory/Defeat:

```text
last meaningful commit, e.g. Damage
→ Victory / Defeat Presentation Record
→ Apply terminal Envelope.FinalSnapshot
```

Resolution fault:

```text
already committed Records
→ ResolutionFault Presentation Record
→ Apply ResolutionFaulted Envelope.FinalSnapshot
```

Presentation terminal ordering must not require BattleState to wait for visual completion.

---

## 12. UI-A2 implementation slices

Do not start with Damage animation. Build the resolution/snapshot/envelope contract first.

### UI-A2A — Resolution / Envelope / frozen-state infrastructure — NEXT

Implement and validate:

```text
Resolution Origin
BattleId / ResolutionId / PresentationSequence
Begin / Abort / Seal lifecycle
immutable FPresentationResolutionEnvelope
self-contained FPresentationStateSnapshot
single Battle-layer PresentationId resolution
resolved-ID uniqueness validation
PresentationUnavailable bootstrap failure
Recorder as current-resolution builder only
OnPresentationResolutionReady
single display owner when presentation is enabled
late-subscribe current frozen baseline
latest-revision-only runtime input binding cache
bounded Controller Envelope queue
presentation-disabled / no-controller gameplay invariance
```

Automation must cover at least:

```text
accepted Request establishes a Resolution before execution
ordinary validation rejection establishes no Resolution
post-validation framework fault seals a fault/system Resolution
BattleStart Resolution begins before fallible opening work
System Resolution path
empty-record Resolution seals correctly
fault preserves already-committed Records
battle restart does not leak old Records/Envelope/IDs
late subscription does not replay an older battle
Frozen Snapshot application performs no historical runtime query
runtime input bindings are unavailable during historical playback
runtime input bindings refresh only after caught-up matching BattleId/Revision
OnReadStateReady cannot directly bypass an active PresentationController
no Controller / presentation disabled leaves Gameplay result unchanged
bounded backlog collapse catches up to the newest FinalSnapshot
```

### UI-A2B — Damage + Block vertical slice

Implement:

```text
FDamageCommitResult
FBlockCommitResult
Damage Record
BlockChanged Record
fully blocked damage
turn-start ClearBlock records
lethal Damage → terminal record ordering
Controller playback token
Skip / stale callback / duplicate callback / missing callback fail-safe
```

Do not record opening-hand initialization Block clear as player-visible history.

### UI-A2C — Card / Deck vertical slice

Implement:

```text
CardPlayed
CardZoneChanged mutation result
CardDrawn
Hand cleanup → Discard
PlayArea → Discard
PlayArea → Exhaust
PlayArea → Removed
successful Shuffle record
Shuffle → DeckShuffled reactions → RetryDraw ordering
```

### UI-A2D — Status + terminal completion

Implement:

```text
Status create / merge / reduce / remove mutation results
Status source/reason presentation context
Victory
Defeat
ResolutionFault
```

Uppercut-like flows should eventually be legible as ordered facts such as:

```text
CardPlayed
→ Damage
→ Weak change
→ Vulnerable change
```

without special-case combo presentation code.

---

## 13. Explicit non-goals for UI-A2

UI-A2 is not final Presentation Polish.

Do not pull these in merely because the committed presentation framework exists:

```text
final card flight animation
drag-and-drop
single-target fast play
target arrows
VFX / SFX polish
rich keyword rendering
advanced prediction
controller/accessibility overhaul
replay database
unbounded presentation history
Gameplay waiting for animation
```

Simple temporary visual playback is sufficient once the architecture is validated.

---

## 14. Acceptance principle

UI-A2 is successful when the same authoritative gameplay result occurs with presentation:

```text
normal speed
fast-forwarded
skipped
disabled
controller/widget temporarily unavailable
```

and when every displayed historical sequence can be explained by one immutable sealed Envelope whose Records and frozen FinalSnapshot describe the same Gameplay resolution.
