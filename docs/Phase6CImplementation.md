# Phase 6C — DeckShuffled Event

Status: **COMPLETE / UE5.8 BUILD + PHASE6C 5/5 + TOTAL 53/53 AUTOMATION PASSED**.

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
→ validate Deck / Queue
→ confirm this is not an expected Deck-level no-op
→ resolve event-dispatch dependencies
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

Event context is deliberately **optional at the generic PlayCard layer**. A Strike/Defend-style card must not gain a new hard failure dependency merely because shuffle events now exist. Draw actions receive the context when available and only require valid battle event wiring if they actually reach the empty-draw → shuffle path.

The pre-6C public call shapes are preserved:

```text
DrawCardAction::Initialize(Deck)
ShuffleDeckAction::Initialize(Deck)
PlayCardAction::Initialize(Battle, Card, Source, Target, Deck)
```

When those legacy entry points genuinely need a successful shuffle, they may resolve the same narrow battle-scoped dispatcher/combatants from the authoritative Queue/BattleManager relationship. They never search the world.

## Failure / no-op semantics

Expected Deck-level no-ops are checked before event wiring becomes mandatory:

```text
DiscardPile empty
DrawPile not empty
```

They remain ordinary no-op/fail-soft Deck outcomes:

```text
no shuffle commit
no DeckShuffled event
no new ResolutionFault merely because event context is absent
```

If a shuffle can commit, valid event wiring is required **before** that commit. Missing/invalid wiring requests Queue `ResolutionFault` instead of committing an unpublishable post-commit fact.

After a successful shuffle commit:

```text
Dispatcher success → continue normally
Dispatcher final reaction insertion failure → ResolutionFault
```

The current Action still calls `Finish()` so the Queue can enter the fault at its normal safe point.

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

Validated gate:

```text
Phase 5    13/13
Phase 6A   23/23
Phase 6B   12/12
Phase 6C    5/5
----------------
Total      53/53
```

The owner-only UE5.8 workflow passed this gate. Phase 6C is complete.

## UE Editor assets

No new `.uasset` or `.umap` configuration is required for this source slice.

Sundial is intentionally not implemented in Phase 6C. Phase 7 must be able to add Sundial as a new trigger source using the `FDeckShuffledEvent` timing established here without changing DeckRuntime shuffle semantics.

## Validation result

The trusted owner-only workflow ran against `main`:

```text
Actions
→ UE Phase 6C Automation
→ Run workflow
→ main
```

Result: **53/53 PASS**.

`AGENTS.md` records Phase 6C as complete. Phase 6R is the next implementation slice.
