# Phase 6C — DeckShuffled Event

Status: **SOURCE IMPLEMENTED / UE5.8 BUILD + AUTOMATION VALIDATION PENDING**.

Phase 6C adds the second real post-commit battle event without introducing Relics or Sundial yet.

## Runtime contract

The existing empty-draw continuation remains:

```text
DrawCardAction
→ [ShuffleDeckAction, RetryDrawAction]
```

A successful shuffle now resolves as:

```text
ShuffleDeckAction Execute
→ validate Deck / Queue / event-dispatch dependencies
→ DeckRuntime::ShuffleDiscardIntoDrawPile()
→ shuffle commit succeeds
→ FDeckShuffledEvent(ExactDeck)
→ BattleEventDispatcher
→ eligible reactions inserted at Queue front
→ ShuffleDeckAction Finish
→ reactions execute
→ RetryDrawAction executes
```

Therefore the required ordering is:

```text
DrawCardAction
→ ShuffleDeckAction commit
→ FDeckShuffledEvent
→ Shuffle reactions
→ RetryDrawAction
```

A failed/no-op `ShuffleDiscardIntoDrawPile()` emits **no** `FDeckShuffledEvent`.

## Typed event representation

`FBattleEvent` now has an explicit internal event type discriminator and two concrete payloads:

```text
FTurnEndedEvent
FDeckShuffledEvent
```

Checked access is isolated by type:

```text
DeckShuffled.TryGet<FDeckShuffledEvent>() → payload
DeckShuffled.TryGet<FTurnEndedEvent>()    → nullptr

TurnEnded.TryGet<FTurnEndedEvent>()       → payload
TurnEnded.TryGet<FDeckShuffledEvent>()    → nullptr
```

`FDeckShuffledEvent` carries the exact `UDeckRuntime*` whose shuffle committed.

Events remain short-lived C++ values. No event UObject/DataAsset and no persistent Trigger Registry were added.

## Explicit dispatch dependencies

The battle-scoped `UBattleEventDispatcher` still receives the authoritative combatants used for current Status trigger collection.

Phase 6C propagates these dependencies through draw/card-play action building. `ABattleManager::TryBuildEventDispatchContext(...)` is a narrow bridge while BattleManager still owns:

```text
EventDispatcher
Player
Enemy
```

Card effects receive only the generic dispatcher/combatant references in `FCardPlayContext`; they do not search actors and do not own trigger membership.

The pre-6C `Initialize(Deck)` and `PlayCardAction::Initialize(...)` call shapes are preserved for compatibility. A direct BattleManager debug draw may resolve the same narrow context from its authoritative Queue owner only when a shuffle is actually required.

## Failure semantics

Before a shuffle commit, invalid event wiring requests a Queue ResolutionFault rather than committing a shuffle that cannot emit its required post-commit event.

After a successful shuffle commit:

```text
Dispatcher success → continue normally
Dispatcher final reaction insertion failure → ResolutionFault
```

The current Action still calls `Finish()` so the Queue can enter the fault at its normal safe point.

Expected deck-level no-ops are not framework faults:

```text
DiscardPile empty
DrawPile not empty
```

They simply emit no event and Finish.

## Regression coverage

No new test-only reflected `UCLASS` was added. Phase 6C reuses the existing `UPhase6ATestRecordTrigger` / `UPhase6ATestRecordAction` helpers and extends them only with DeckShuffled recording behavior, preserving the Phase 6R extraction guardrail.

New prefix:

```text
SlayTheSpireDemo.Phase6C
```

Exactly 5 tests are expected:

```text
Event.TypedPayloadIsolation
Shuffle.SuccessEmitsAfterCommit
Shuffle.EmptyDiscardDoesNotEmit
Shuffle.NonEmptyDrawPileDoesNotEmit
Draw.ShuffleReactionBeforeRetryDraw
```

The strongest ordering test records DrawPile count at actual Action execution time:

```text
successful shuffle commit → DrawPile=1
DeckShuffled reaction     → records 1
RetryDrawAction            → consumes card
post-retry tail Action     → records 0
```

Expected record:

```text
[1, 0]
```

The same test also requires exactly one final `QueueEmpty` for the whole Draw → Shuffle → Reaction → RetryDraw resolution.

## UE5.8 CI gate

New workflow:

```text
.github/workflows/ue-phase6c-tests.yml
```

It remains owner-only, manual `workflow_dispatch`, trusted `main`, self-hosted Windows `ue58`.

Expected gate after source compiles:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
----------------
Total      53/53
```

Do not mark Phase 6C complete until this gate passes.

## UE Editor assets

No new `.uasset` or `.umap` configuration is required for this source slice.

Sundial is intentionally not implemented in Phase 6C. Phase 7 must be able to add Sundial as a new trigger source using the `FDeckShuffledEvent` timing established here without changing DeckRuntime shuffle semantics.

## Next validation step

Run:

```text
Actions
→ UE Phase 6C Automation
→ Run workflow
→ main
```

Expected result: **53/53 PASS**.

After that result is confirmed, synchronize `AGENTS.md` to mark Phase 6C complete and advance to Phase 6R.
