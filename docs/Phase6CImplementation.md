# Phase 6C — DeckShuffled Event

Status: **COMPLETE / HISTORICAL PHASE 6C VALIDATION SEALED**.

Phase 6C added the second real post-commit battle event. Phase 7C later refined one gameplay-fidelity edge of the shuffle contract so the demo matches the intended Slay-the-Spire Sundial draw loop.

## Phase 7C gameplay-fidelity amendment

The original Phase 6C rule treated `DiscardPile empty` as a shuffle no-op. That rule is superseded for **gameplay draw attempts**.

The current authoritative rule is:

```text
one authored DrawCardAction / one draw attempt
↓
DrawPile non-empty
→ draw normally
→ no shuffle

DrawPile empty
→ commit exactly one gameplay Shuffle attempt
→ DiscardPile cards, if any, move to DrawPile
→ DiscardPile may also be empty: MovedCardCount = 0 is still a committed gameplay shuffle
→ emit DeckShuffled Record/Event
→ run shuffle reactions
→ RetryDraw exactly once
→ if DrawPile is still empty, this draw attempt ends
```

Therefore an upgraded draw-two card can produce two shuffle facts when appropriate:

```text
before Draw #1: Draw=0, Discard=1
Draw #1
→ shuffle 1 card
→ DeckShuffled #1
→ draw that card

before Draw #2: Draw=0, Discard=0
Draw #2
→ zero-card gameplay shuffle
→ DeckShuffled #2
→ RetryDraw remains empty and ends
```

This is required for the generic two-Pommel-Strike+/Sundial interaction. There is still no Pommel Strike or Sundial special case in Deck/Draw code.

The following remain unchanged:

```text
initial battle setup shuffle emits no gameplay DeckShuffled event
DrawPile non-empty rejects ShuffleDiscardIntoDrawPileCommit
one draw attempt may shuffle at most once
zero-card RetryDraw never recursively shuffles forever
DeckShuffled still occurs after the shuffle commit and before RetryDraw
```

## Runtime contract

The empty-draw continuation is:

```text
DrawCardAction
→ [ShuffleDeckAction, RetryDrawAction]
```

A committed gameplay shuffle resolves as:

```text
ShuffleDeckAction Execute
→ validate Deck / Queue
→ require DrawPile empty
→ resolve event-dispatch dependencies
→ DeckRuntime::ShuffleDiscardIntoDrawPileCommit()
→ shuffle commit succeeds
   - MovedCardCount may be > 0
   - MovedCardCount may be 0 when DiscardPile is empty
→ FDeckShuffledEvent(ExactDeck)
→ BattleEventDispatcher
→ eligible reactions inserted at Queue front
→ ShuffleDeckAction Finish
→ reactions execute
→ RetryDrawAction executes once
```

Required ordering remains:

```text
DrawCardAction
→ ShuffleDeckAction commit
→ FDeckShuffledEvent
→ Shuffle reactions
→ RetryDrawAction
```

A shuffle with a **non-empty DrawPile** remains rejected and emits no `FDeckShuffledEvent`.

## Typed event representation

`FBattleEvent` has an explicit internal event type discriminator and concrete payloads including:

```text
FTurnEndedEvent
FDeckShuffledEvent
```

Checked access remains type-isolated. `FDeckShuffledEvent` carries the exact `UDeckRuntime*` whose gameplay shuffle committed.

Events remain short-lived C++ values. No event UObject/DataAsset and no persistent Trigger Registry is required.

## Explicit dispatch dependencies

The battle-scoped `UBattleEventDispatcher` receives the authoritative battle context and combatants used for Trigger collection.

`ABattleManager::TryBuildEventDispatchContext(...)` remains the narrow bridge. Card effects receive dispatcher/combatant references through their play context; they do not search actors and do not own trigger membership.

When a draw reaches an empty DrawPile, valid event wiring is required before the gameplay shuffle commit. This now includes a zero-card shuffle. Missing/invalid wiring requests Queue `ResolutionFault` instead of committing an event-producing fact that cannot be dispatched.

## Failure / no-op semantics

Current semantics:

```text
DrawPile not empty
→ no shuffle commit
→ no DeckShuffled event

DrawPile empty, DiscardPile non-empty
→ committed shuffle
→ cards move
→ DeckShuffled event

DrawPile empty, DiscardPile empty
→ committed zero-card gameplay shuffle
→ no card movement
→ DeckShuffled event
```

A `RetryDrawAction` created by the same draw attempt is marked as already having performed its one shuffle. If it still sees an empty DrawPile, it finishes without scheduling another shuffle.

## Regression coverage

Historical Phase 6C validation remains valid evidence for the original event ordering and dispatcher contracts. The Phase 7C correction adds focused coverage for the newly discovered gameplay-fidelity edge:

```text
SlayTheSpireDemo.Phase7.Sundial.DrawTwoCountsZeroCardShuffle
```

That regression must prove:

```text
Draw=0, Discard=1
+ two sequential draw attempts
→ first shuffle moves one card
→ first RetryDraw draws it
→ second draw commits a zero-card shuffle
→ exactly two Sundial shuffle counts
→ second RetryDraw terminates without recursion/fault
```

The existing Phase 6C ordering contract remains sticky unless this amendment causes a concrete regression.

## Historical Phase 6C validation

The original owner-only UE5.8 gate passed:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
----------------
Total      53/53
```

That historical gate is not reinterpreted as validation of the later Phase 7C amendment. The amendment receives its own current-main Build/focused Automation evidence before Phase 7C is sealed.
