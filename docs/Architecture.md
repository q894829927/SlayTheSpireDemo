# Architecture

This document is the durable architectural overview. Directory-level `AGENTS.md` files define implementation rules; detailed current Presentation contracts remain in the Phase 6UI-A2 documents.

## 1. Battle Execution

```text
CardData / CardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ typed Operation Spec
→ Modifier Pipeline
→ Commit
→ BattleEvent
→ Trigger collection
→ Reaction BattleActions
→ BattleActionQueue
```

`BattleStateMachine` controls macro turn flow. `BattleActionQueue` controls deterministic execution order. Modifiers change an operation before commit. Events describe facts after commit. Triggers build queued reactions.

Complex behavior must emerge from generic composition. Pommel Strike knows configured Damage/Draw effects; Defend knows Block; DeckRuntime knows zones/draw/shuffle; future Sundial knows shuffle events. None should know concrete combinations.

## 2. State Ownership

### BattleManager

Owns battle orchestration, turn transitions, battle-scoped identity/RNG allocation, public Query/Request boundaries and stable read publication.

### Combatants

Own authoritative HP and Block. Typed CommitResults carry before/after facts to Actions without making Combatants depend on Presentation.

### DeckRuntime

Owns DrawPile, Hand, DiscardPile, ExhaustPile and PlayArea truth. DrawPile end is top. Runtime card identity is stable `UCardInstance` identity; RuntimeId is presentation/debug identity.

### StatusContainer

Owns authoritative Status membership and merge/create decisions. `UStatusInstance` owns mutable Amount, RuntimeSequence and Owner; `UStatusData` is immutable definition data.

## 3. BattleActionQueue

Only one authoritative Action executes at a time. Ordering and completion are explicit. Actions may enqueue dependencies but never drive queue advancement.

Dependent batches for one logical chain are inserted before the current action finishes. Nested reactions use queued depth-first semantics. Queue faults enter at safe points, broadcast once, suppress normal QueueEmpty and reject further mutation.

QueueEmpty is an observable non-reentrant boundary. BattleManager defers macro turn continuation until all observers return.

## 4. Modifier Pipelines

```text
ActionQueue       → execution timing/order
Modifier Pipeline → pre-commit modification/interception/override/clamp
BattleEvent       → post-commit fact
Trigger           → post-commit reaction that builds Actions
```

Use typed specs such as `FDamageSpec` and `FBlockSpec`. Avoid a universal modifier context.

Deterministic ordering within a domain is:

```text
Phase → Priority → RuntimeSequence → LocalModifierIndex
```

Ratio arithmetic uses explicit integer numerator/denominator, safe intermediates and floors after every modifier.

## 5. Events and Triggers

A BattleEvent is a short-lived immutable-by-contract value fact. Dispatch/Trigger code must not cache its references.

Trigger sources are collected on demand. Eligibility uses snapshot semantics; built Actions validate live state at Execute-time. Trigger ordering is `Priority → RuntimeSequence → LocalTriggerIndex`.

Triggers are read-only builders. They never mutate Gameplay or drive the queue. Event emission follows a successful commit, and reaction batches are atomically inserted before the source action finishes.

`FDeckShuffledEvent` occurs only after a real discard-to-draw shuffle commit and before RetryDraw. Initial battle setup shuffle is normalization, not a Gameplay event.

## 6. Card and Deck Resolution

```text
UCardData
→ UCardInstance
→ UPlayCardAction
→ UCardEffect::BuildActions() const
→ effect Actions
→ UFinishCardPlayAction
→ Execute-time destination resolution
```

Effects are immutable shared definitions that capture base intent. Mutable-state-dependent outcomes resolve at Action Execute-time. FinishCardPlay delegates authoritative movement to DeckRuntime.

Draw does not synchronously shuffle. One DrawAction is one draw attempt. Empty DrawPile with available Discard schedules `Shuffle → RetryDraw`. Battle RNG is initialized once and consumed deterministically.

## 7. Presentation Architecture

Gameplay and Presentation are independent timelines:

```text
Gameplay validation / Request
→ Begin Presentation Resolution
→ BattleActionQueue
→ Gameplay Commit
→ immutable typed Presentation Records
→ Gameplay/macro stability
→ freeze exact FPresentationStateSnapshot
→ seal immutable Resolution Envelope
→ deferred public delivery
→ BattlePresentationController
→ ViewModel working state
→ UMG playback
→ apply matching Envelope.FinalSnapshot
```

A sealed Envelope owns one `BattleId`, `ResolutionId`, Origin, ordered Records, FinalStateRevision and matching FinalSnapshot. Historical playback never reconstructs the past from mutable Gameplay.

Internal seal and public notification are distinct. Seal releases the builder before another Resolution begins; `OnPresentationResolutionReady` and `OnReadStateReady` remain deferred until after an accepted Request returns.

See `docs/Phase6UIA2Implementation.md` for the complete contract and `docs/Phase6UIA2EImplementation.md` for current Blueprint/PIE closure.

## 8. MVVM Boundaries

```text
MODEL
BattleManager / Combatants / DeckRuntime / Status runtime / Enemy Intent / BattleActionQueue

VIEWMODEL
frozen player-facing display state
formal Request forwarding
presentation-only selection/focus
latest-only live input bindings

VIEW
UMG Widgets
```

`FBattleReadSnapshot` is a coherent current Gameplay/read structure and may hold weak runtime references. `FPresentationStateSnapshot` is the frozen display model for one exact revision and has no mutable Gameplay dependency.

Latest-only runtime bindings map current RuntimeId/TargetId to weak objects solely for formal Request submission after Presentation catches up. They are not historical state.

## 9. Public Read and Request Boundary

Normal UI uses formal Query/Request APIs. Query is advisory; Request revalidates current authoritative state. `AcceptedForResolution` does not mean playback has completed.

Widgets do not use QueueEmpty/idle as their public completion protocol. BattleManager publishes coherent battle-level Ready edges. Public notifications never fire re-entrantly before the originating accepted Request returns.
