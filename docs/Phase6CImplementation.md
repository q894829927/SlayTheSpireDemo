# Phase 6C — DeckShuffled Event

Status: **COMPLETE / HISTORICAL PHASE 6C VALIDATION SEALED**.

Phase 6C added the post-commit `DeckShuffled` event. Phase 7C later refined the draw-side producer contract after Sundial exposed an important source-game behavior: `Draw N` must remain one bulk request instead of being flattened into N unrelated draw attempts.

## Phase 7C gameplay-fidelity amendment

Current draw ownership is:

```text
UDrawCardEffect(DrawCount = N)
→ UDrawCardsAction(N)
   - owns RemainingDraws
   - inspects current DrawPile / DiscardPile / Hand capacity
   - plans deterministic continuation batches
→ UDrawCardAction
   - atomic one-card DrawPile -> Hand commit only
→ UShuffleDeckAction
   - atomic shuffle commit + DeckShuffled Record/Event
→ UDrawCardsAction(Remaining)
```

`UDrawCardAction` no longer decides whether to shuffle and no longer recursively retries itself. Shuffle planning belongs only to `UDrawCardsAction`.

## Bulk draw semantics

A fresh bulk request first checks whether any card exists at all:

```text
DrawPile = 0
DiscardPile = 0
→ DrawCardsAction ends
→ no ShuffleAction is scheduled
→ no DeckShuffled event
```

When cards exist but the current DrawPile cannot satisfy the whole request:

```text
ImmediateDraws = min(RemainingDraws, DrawPileCount)
RemainingAfterImmediate = RemainingDraws - ImmediateDraws

plan atomically:
[DrawCardAction x ImmediateDraws]
→ ShuffleDeckAction
→ DrawCardsAction(RemainingAfterImmediate)
```

When `DrawPile = 0` and `DiscardPile > 0`, `ImmediateDraws = 0`, so the continuation is simply:

```text
ShuffleDeckAction
→ DrawCardsAction(same RemainingDraws)
```

The remaining bulk action re-evaluates the live deck state when it executes.

## Why a zero-card shuffle can still be real

A zero-card shuffle is not produced merely because a new draw request sees an empty deck. It occurs only when an earlier bulk-draw planning step already scheduled a `ShuffleDeckAction` while the request still owed more draws.

Example:

```text
Draw 2
initial: Draw=0, Discard=1

BulkDraw(2)
→ plans Shuffle #1 + BulkDraw(2)

Shuffle #1
→ moves the one discard card to Draw
→ DeckShuffled #1

BulkDraw(2)
current: Draw=1, Discard=0
→ plans DrawCard(1) + Shuffle #2 + BulkDraw(1)

DrawCard(1)
→ consumes the only Draw card

Shuffle #2 executes as already planned
current: Draw=0, Discard=0
→ commits with MovedCardCount=0
→ DeckShuffled #2

BulkDraw(1)
current: Draw=0, Discard=0
→ ends without scheduling a third shuffle
```

This is the generic behavior required for the two-Pommel-Strike+/Sundial loop. There is no card identity or Relic identity check in Draw/Deck code.

## Shuffle commit semantics

`UShuffleDeckAction` may commit when the authoritative DrawPile is empty. `DiscardPile` may be empty if the action was already scheduled by a prior bulk-draw step.

```text
DrawPile non-empty
→ ShuffleDeckAction no-op
→ no DeckShuffled

DrawPile empty, DiscardPile non-empty
→ move Discard -> Draw
→ shuffle with battle RNG
→ committed DeckShuffled

DrawPile empty, DiscardPile empty
→ MovedCardCount = 0
→ committed DeckShuffled
```

The event ordering remains unchanged:

```text
Shuffle commit
→ DeckShuffled Presentation Record when a writer is available
→ FDeckShuffledEvent(ExactDeck)
→ Trigger reactions inserted at Queue front
→ reactions execute
→ remaining bulk draw continues
```

Initial battle deck setup randomization still emits no gameplay `FDeckShuffledEvent`.

## Hand capacity

`UDrawCardsAction` caps the live request to available Hand slots before planning. A full Hand ends the bulk request without scheduling additional draw/shuffle work.

## Explicit dispatch dependencies

Bulk draw carries the existing dispatcher/combatant context when available. It only requires valid event wiring when it actually needs to schedule a shuffle-producing continuation. No actor search or persistent Trigger Registry is introduced.

## Regression coverage

Historical Phase 6C evidence remains sealed for the original event/ordering layer. The corrected current contract is covered by:

```text
SlayTheSpireDemo.Phase6C.Event.TypedPayloadIsolation
SlayTheSpireDemo.Phase6C.Shuffle.SuccessEmitsAfterCommit
SlayTheSpireDemo.Phase6C.Shuffle.EmptyDiscardDoesNotEmit
SlayTheSpireDemo.Phase6C.Shuffle.NonEmptyDrawPileDoesNotEmit
SlayTheSpireDemo.Phase6C.Draw.EmptyBulkDoesNotShuffle
SlayTheSpireDemo.Phase6C.Draw.ShuffleReactionBeforeRetryDraw
```

The Sundial integration additionally proves:

```text
SlayTheSpireDemo.Phase7.Sundial.DrawTwoCountsZeroCardShuffle
```

That test uses one `UDrawCardsAction(2)`, not two independent single-draw actions.

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

That historical total is not reinterpreted as evidence for the later bulk-draw amendment. The amended contract requires a new current-head Build plus the directly affected focused Phase6C and Phase7 Sundial gates before Phase 7C can be sealed.
