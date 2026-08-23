# Phase 6UI-A2 — Basic Committed Presentation

Status: **C++ COMMITTED-PRESENTATION COMPLETE / VALIDATED / SEALED; BLUEPRINT/PIE NEXT IN A2E**.

UI-A2 replaces the UI-A0/UI-A1 immediate/no-op presentation catch-up boundary with deterministic playback of already-committed gameplay facts. It does not make `BattleActionQueue`, `BattleState` or authoritative gameplay wait for animation.

UI-A2A establishes transport, resolution, freezing, failure and playback-safety infrastructure; A2B/A2C add committed Damage/Block and Card/Energy/Zone/Shuffle facts; A2D adds Status and formal terminal/fault committed presentation. The player-visible Blueprint/UMG closure is owned by `UI-A2E — Unified Blueprint Playback & PIE Acceptance`.

### Current implementation / validation status

Owner-confirmed final C++/Automation evidence:

```text
UI-A2A C++ / Automation             PASSED 8/8
UI-A2B C++ / Automation             PASSED 8/8
UI-A2C C++ / Automation             PASSED 8/8
UI-A2D1                             PASSED 3/3
UI-A2D2                             PASSED 4/4
UI-A2D3                             PASSED 4/4
UI-A2D4                             PASSED 6/6
UI-A2D5 focused                     PASSED 6/6
Phase6R expanded aggregate          PASSED 100/100
Shipping exclusion                  PASS
Unified Blueprint/UMG playback      NEXT — UI-A2E
PIE committed-presentation smoke    NEXT — UI-A2E
```

The exact discovered totals are run evidence, not permanent architecture constants. C++/Automation validation does not claim the remaining Blueprint/UMG or PIE work.

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

`FPresentationStateSnapshot` may keep immutable presentation assets such as textures. It must not depend on mutable Gameplay runtime identities such as `UCardInstance`, `UStatusInstance`, `ACombatant` or `ABattleManager`.

Applying a frozen snapshot must not call current Gameplay APIs to rebuild historical card metadata, status text, playability, target legality, HP/Block/Energy or pile state.

### 2.2 Historical display and live input identity are separate

The current formal Request API still needs runtime objects:

```text
RequestPlayCard(UCardInstance*, ACombatant*)
```

UI-A2 keeps a separate, non-authoritative input-binding cache only for the newest caught-up revision:

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

The cache exists only to forward current user intent to formal Gameplay Requests. Requests still revalidate authoritative state.

### 2.3 Seal lifecycle and public notification lifecycle are separate

`OnReadStateReady(BattleId, StateRevision)` remains a non-replaying edge meaning that a stable current state can be read. It is not a historical payload.

Internal stability must synchronously complete:

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
append sealed Envelope to battle-scoped pending-public-delivery FIFO
```

More than one Resolution may Seal before the deferred public callback runs. Therefore this handoff is a FIFO, not one overwriteable `PendingEnvelope`.

The FIFO is battle-scoped and bounded, preserves ResolutionId order, is cleared on battle restart, never delivers an old BattleId and does not coalesce Envelopes merely because current read-state notification advances to a newer StateRevision. Overflow is Presentation-only degradation; it never requests Gameplay ResolutionFault.

Public callbacks happen later:

```text
originating public Request returns
↓
next safe deferred public-notification boundary
↓
drain pending-public-delivery FIFO in ResolutionId order
↓
OnPresentationResolutionReady(each sealed Envelope)
↓
OnReadStateReady(BattleId, StateRevision)
```

Neither public callback may re-enter an accepted public Request before that Request returns.

Display ownership is singular:

```text
Presentation enabled
→ Presenter/Controller drives ViewModel display using frozen snapshots
→ ViewModel does not Pull+Apply live state from OnReadStateReady

Presentation disabled / no Controller
→ apply the same newest frozen baseline immediately at the public presentation boundary
→ no historical playback required
```

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

`PresentationSequence` is battle-scoped deterministic ordering and is independent of StateRevision.

### 3.2 Origins

Current origins:

```text
BattleStart
PlayCard
EndTurn
System
```

Do not predeclare AI/Relic/Replay origins until a real caller needs them.

### 3.3 End Turn macro flow

The complete automatic End Turn progression is one Resolution:

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
Gameplay for N may resolve synchronously
↓
internal stable boundary seals or aborts N
↓
active builder is released
↓
only then may BeginResolution(N+1) succeed
```

Deferred public delivery does not keep Resolution N active. Sealed-but-not-yet-delivered Envelopes are immutable entries in the bounded pending-public-delivery FIFO.

If another Begin is attempted while a builder is still legitimately active, the implemented fail-safe rejects the second Begin, clears the stale presentation-only builder and degrades Presentation for that battle rather than overwriting it. Gameplay remains unchanged.

### 3.5 Fault lifecycle belongs to UI-A2A

`ResolutionFault` Record lifecycle is infrastructure.

If a framework fault occurs after the builder exists:

```text
already committed Records remain
→ append ResolutionFault as final Record
→ freeze ResolutionFaulted FinalSnapshot
→ Seal once
→ release builder
```

Once ResolutionFault has been appended, any later append invalidates the whole unpublished batch.

A2D later formalized the typed fault payload, terminal reducer semantics and visible treatment contract; A2D5 validated a genuine queue/framework fault path.

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

Actions that can create player-facing committed facts receive/use a narrow optional battle-scoped RecordWriter/Sink through explicit initialization/context propagation.

Hard rules:

```text
Action must not world-search for BattleManager/Recorder
Action must not use GetAllActorsOfClass
Action must not infer Recorder by casting UObject Outer
Action must not use a global/singleton Recorder
Gameplay Runtime owners must not depend on RecordWriter
```

Nested/reaction Actions in one active Gameplay Resolution receive the same writer through their explicit action-building/dispatch context path.

### 5.3 Writer absence and append failure have different semantics

Writer absent from the start is valid no-history mode. Gameplay runs normally and the final frozen baseline is enough.

If a writer exists and an Append unexpectedly fails after earlier Records may already have been accepted:

```text
mark current builder invalid
↓
discard every buffered-but-unpublished Record for that Resolution
↓
do not Seal/Publish partial historical Envelope
↓
continue Gameplay Commit / Action Finish / Queue ordering
↓
Freeze exact final baseline at stability when possible
↓
PresentationUnavailable / fail-safe catch-up
↓
never Gameplay ResolutionFault
```

---

## 6. Freeze, Seal, identity and de-duplication policy

Envelope identity and read-state public-edge identity are deliberately different:

```text
Envelope identity / de-duplication
= (BattleId, ResolutionId)

Read-state public edge
= Gameplay (BattleId, StateRevision) plus Presentation availability state
```

One active ResolutionId seals at most once. One sealed `(BattleId, ResolutionId)` publishes at most once. Duplicate stable read callbacks do not re-Seal historical Envelopes.

A2D5 review fixed the Presentation-only edge case where availability changes at an unchanged Gameplay revision. A `PresentationAvailable true -> false` transition must remain observable by Controller/ViewModel even when BattleId/StateRevision do not change; identical subsequent edges may still deduplicate.

If raw read succeeds but Freeze fails, or Envelope construction/Seal fails:

```text
clear/discard current Presentation builder safely
mark Presentation unavailable/disabled for this battle
allow ordinary Gameplay and deferred public read behavior to continue
never request Gameplay ResolutionFault
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

Damage commit truth contains incoming damage, HP before/after, Block before/after, blocked damage and HP damage.

The Damage Record is the single presentation fact for damage absorption and HP loss. Damage consuming Block must not emit another BlockChanged Record for the same commit.

### 7.2 Block

Block gain/clear commit truth contains Block before/after and changed amount.

```text
StartOpeningHand ClearBlock → setup normalization → no visible Record
StartPlayerTurn / StartEnemyTurn ClearBlock → BlockChanged(TurnStartClear)
```

### 7.3 Energy / CardPlayed

CardPlayed preserves exact energy history:

```text
CardRuntimeId
CardId
SourcePresentationId
TargetPresentationId when applicable
EnergyBefore
EnergyAfter
CostPaid
```

Card cost lives only in CardPlayed. Do not emit duplicate EnergyChanged for the same cost.

### 7.4 Deck/card zones

DeckRuntime returns generic real zone-mutation facts. Action-level presentation maps them to `CardZoneChanged` and related committed history. Shuffle is recorded only after a real discard-to-draw shuffle commit.

Initial setup shuffle and BattleStart opening-Hand draws emit no visible shuffle/draw Records. Normal BattleStart may therefore seal an empty-record Envelope whose FinalSnapshot establishes the visible baseline.

### 7.5 Status

A2D implements a typed status mutation result with exact RuntimeSequence identity, AmountBefore/After, create/remove flags and effective runtime definition/instance for synchronous freezing only.

Apply/Reduce/Remove no-op or stale exact-instance paths emit no StatusChanged Record. Presentation freezes dynamic Before/After text and icon metadata from the true effective definition.

---

## 8. Frozen presentation state model

`FPresentationStateSnapshot` is the complete display model used by immediate baseline rendering and sealed Envelopes. It freezes all current HUD values needed to apply one exact revision without re-entering mutable Gameplay.

It includes Battle identity/state/outcome, Energy, bCanEndTurn, Player/Enemy combatant display state, ordered statuses with RuntimeSequence, ordered Hand cards, pile counts and committed Enemy Intent presentation.

Selection, hover, feedback, legal-target runtime bindings and PlaybackToken remain transient presentation/input state and are not historical snapshot state.

---

## 9. Unified resolved PresentationId

One Battle-layer resolver owns resolved participant presentation identity. Snapshot, LegalTargets and Presentation Records use the same resolved value.

Rules:

```text
explicit authored PresentationId non-empty → use it
Combatant == Player → fallback Player
Combatant == current Enemy → fallback EnemyPrimary
not part of current battle → fail
```

Resolved IDs are non-empty, battle-scoped unique and locked for the battle lifetime after the first exact frozen baseline.

---

## 10. Presentation bootstrap failure

Invalid resolved PresentationId, Freeze bootstrap failure or other Presentation-only initialization failures do not become Gameplay ResolutionFaulted.

Selected policy:

```text
PresentationUnavailable
→ ViewModel initialization succeeds into explicit UI-only error state
→ Presenter still creates normal HUD Widget
→ normal player input disabled
→ visible development-facing error surface
→ headless Gameplay correctness unchanged
```

---

## 11. Controller / Presenter ownership

`ABattleHUDPresenter` remains the assembly point for ViewModel, PresentationController and HUD Widget. `OnReadStateReady` is not a second HUD display owner.

### 11.1 Bounded Envelope queue

Controller owns sealed backlog after public delivery. Each Envelope consumes its own Records then applies its own FinalSnapshot.

First implementation policy remains intentionally small:

```text
fixed queue bound
+ FIFO ordering
+ deterministic overflow collapse to newest retained FinalSnapshot
```

No ACK protocol, persistence, priority scheduling, per-Origin queue policy, producer backpressure, adaptive batching or general presentation scheduler is introduced without a concrete need.

### 11.2 PlaybackToken belongs to UI-A2A

Generic safety protocol:

```text
PlayPresentationRecord(Record, PlaybackToken)
↓
NotifyPresentationFinished(PlaybackToken)
```

Controller ignores duplicate/stale/old-Battle/post-Skip callbacks, handles timeout/missing callback/Widget loss safely and never advances Gameplay because Presentation finished.

A2E now owns wiring the real WBP to this already-validated protocol.

---

## 12. Slice ownership — no cross-phase ambiguity

### UI-A2A — infrastructure — C++ VALIDATED / 8/8

Owns generic Record/Envelope transport, Begin/Abort/Seal lifecycle, fault lifecycle, frozen snapshot, explicit optional writer propagation, no-partial-history policy, deferred publication, PresentationUnavailable, Presenter/Controller display ownership, bounded backlog, PlaybackToken, skip/timeout/stale callback safety and newest-only input-binding refresh.

### UI-A2B — Damage + Block — C++ VALIDATED / 8/8

Owns committed Damage/Block facts, fully blocked damage, damage-consumed Block inside Damage only, TurnStartClear and lethal Damage terminal ordering.

### UI-A2C — Card / Energy / Zone / Shuffle — C++ VALIDATED / 8/8

Owns CardPlayed with exact energy cost history, CardZoneChanged, draw/discard/exhaust/remove facts and Shuffle → reactions → RetryDraw ordering.

### UI-A2D — Status + formal terminal/fault presentation — C++/AUTOMATION SEALED

Owns exact status mutation truth/identity, StatusChanged reasons/frozen values, status historical reducer, typed Victory/Defeat/ResolutionFault terminal semantics and combined acceptance.

Final A2D evidence:

```text
A2D5 focused      6/6 PASS
Phase6R aggregate 100/100 PASS
Shipping          PASS
```

### UI-A2E — Unified Blueprint Playback & PIE Acceptance — NEXT

Owns the concrete WBP route for all visible A2 Record types, token completion wiring, minimal diagnostic playback, exact FinalSnapshot catch-up, input unlock timing, terminal surfaces, PresentationUnavailable separation and PIE end-to-end acceptance.

---

## 13. Internal sealing vs deferred public publication

### 13.1 Internal Gameplay-stable sealing boundary

When Queue/macro flow becomes stable, sealing is synchronous Gameplay-side bookkeeping and finishes before another Resolution may begin:

```text
Queue/macro flow stable
↓
Build raw player-facing read snapshot
↓
Freeze exact FPresentationStateSnapshot
↓
Seal valid active Envelope or keep only baseline for valid no-history mode
↓
clear/release active builder
```

### 13.2 Deferred public-notification boundary

```text
originating public Request has returned
↓
public deferred callback/ticker boundary
↓
drain pending-public-delivery FIFO in ResolutionId order
↓
OnPresentationResolutionReady(each Envelope)
↓
OnReadStateReady(current public read edge)
```

The public delivery FIFO and Controller playback queue are separate bounded queues. Neither is a history database.

### 13.3 De-duplication ownership

```text
Envelope Seal/Publish de-dup
→ (BattleId, ResolutionId)

ordinary public read-edge de-dup
→ Gameplay revision + Presentation availability state
```

A duplicate read callback must not cause another Envelope Seal. Presentation-availability transitions must not be suppressed just because Gameplay revision is unchanged.

---

## 14. C++ Automation closure

The earlier A2A/A2B/A2C focused gates remain valid, and A2D completed the Status/terminal combined acceptance.

Final owner-confirmed closure:

```text
A2D1 3/3 PASS
A2D2 4/4 PASS
A2D3 4/4 PASS
A2D4 6/6 PASS
A2D5 focused 6/6 PASS
Phase6R expanded aggregate 100/100 PASS
Shipping exclusion PASS
```

This closes the C++ committed-presentation contract. It does not claim real Blueprint/UMG animation or PIE historical playback.

---

## 15. USER ACTION REQUIRED boundary

UI-A2E requires real UE Editor work for the concrete Widget Blueprint/UMG surface. Text-only source tooling must not claim to create or edit `.uasset` / `.umap` assets.

When A2E begins, instructions must specify exact Widget assets/events, Record routing, token callback wiring, minimum visible behavior and PIE scenarios.

---

## 16. Acceptance principle

UI-A2 is successful when presentation is a deterministic, bounded, fail-safe explanation of facts that Gameplay already committed.

The C++ contract is sealed. UI-A2 as a whole is **not yet complete** because A2E player-visible Blueprint/PIE acceptance remains.

Final invariant:

```text
Gameplay result
= independent of Presentation speed / availability / callback correctness

Historical display
= driven only by sealed Records + that Envelope.FinalSnapshot

Resolution lifecycle
= internal Seal releases builder before any next Begin; public delivery may be deferred

Envelope identity
= BattleId + ResolutionId

Read-state public edge
= current Gameplay revision plus Presentation availability transition semantics

Current input identity
= refreshed only after display catches up to newest matching BattleId/Revision
```

Next stage:

```text
UI-A2E Unified Blueprint Playback
→ UI-A2E PIE end-to-end acceptance
→ UI-A2 COMPLETE / SEALED
→ resume UI-A3-2 Target-Specific Current-State Preview
```
