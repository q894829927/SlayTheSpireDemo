# Phase 6B — Battle Turn Wiring

Status: **GAMEPLAY SLICE PASSED / QUEUE CONTRACT FIXES PENDING**. The hardened 42/42 gate and PIE cycle passed before six additional QueueEmpty continuation contract regressions were added. Two new tests intentionally expose the current empty-batch and empty-callable defects; Phase 6C remains blocked until those fixes make the expanded 48/48 gate green.

## Runtime wiring implemented

Phase 6B connects the Phase 6A event/trigger infrastructure to authoritative battle turn flow.

```text
PlayerTurn
→ EndPlayerTurn
→ atomically enqueue [TurnEndedAction(Player)]
→ commit PlayerTurnEnding + Energy=0
→ process TurnEndedAction
→ TurnEndedEvent(Player)
→ reactions
→ one final QueueEmpty for the player-ending boundary
→ after all QueueEmpty observers return, start EnemyTurn

StartEnemyTurn
→ atomically enqueue [EnemyDamageAction, TurnEndedAction(Enemy)]
→ commit EnemyTurn + clear enemy Block
→ enemy damage
→ TurnEndedAction enters EnemyTurnEnding
→ TurnEndedEvent(Enemy)
→ reactions
→ one final QueueEmpty for the enemy-ending boundary
→ after all QueueEmpty observers return, start PlayerTurn
```

Implemented battle states:

```text
BattleStart
PlayerTurn
PlayerTurnEnding
EnemyTurn
EnemyTurnEnding
Victory
Defeat
ResolutionFaulted
```

`ABattleManager` creates and owns one transient `UBattleEventDispatcher` for the battle and subscribes to `UBattleActionQueue::OnResolutionFaulted`.

`UTurnEndedAction` is a production BattleAction. It does not directly start the next turn. It coordinates the post-commit event-emission point and lets final `QueueEmpty` own macro turn progression.

## Transactional rules

Player turn ending:

- build the complete player turn-ending batch first;
- call `AddBatchToBackPreserveOrder`;
- only after successful insertion commit `PlayerTurnEnding` and zero Energy;
- insertion/start failure requests `ResolutionFault`.

Enemy turn:

- build `[EnemyDamageAction, TurnEndedAction]` first;
- insert the complete batch atomically;
- only after successful insertion commit `EnemyTurn` and clear enemy Block;
- no partial enemy action may execute after a rejected batch.

A lethal action earlier in the enemy batch suppresses the normal enemy `TurnEndedEvent`. Victory/Defeat remains resolved at the real final `QueueEmpty` instead of from the sentinel.

Queue `ResolutionFault` transitions the BattleManager to `EBattleState::ResolutionFaulted`, sets Energy to zero, and prevents normal turn progression.

## QueueEmpty non-reentrancy hardening — PASSED

A post-6B review identified that synchronous macro progression inside `OnQueueEmpty.Broadcast()` could recursively run the next resolution before later QueueEmpty listeners had observed the current boundary.

The hardened contract is:

```text
Resolution A becomes empty
→ Queue keeps the pump frame active
→ OnQueueEmpty.Broadcast()
    → BattleManager records exactly one deferred authoritative continuation
    → UI / debugger / other observers see the completed A boundary state
→ Broadcast fully returns
→ deferred continuation runs
→ next authoritative batch is enqueued
→ StartProcessing reports success without recursively entering PumpQueue
→ the existing PumpQueue frame continues Resolution B
→ Resolution B QueueEmpty is therefore not nested inside A's multicast
```

`UBattleActionQueue::DeferUntilAfterQueueEmptyBroadcast(...)` accepts at most one authoritative continuation for a QueueEmpty boundary. Direct Action insertion during the QueueEmpty observer multicast is rejected; authoritative producers must first defer macro progression.

`ABattleManager::HandleActionQueueEmpty()` no longer calls `StartEnemyTurn()` / `StartPlayerTurn()` synchronously. It defers those transitions until every observer of the current boundary has returned.

This preserves the observable boundary order independently of multicast registration order:

```text
first QueueEmpty callback  → BattleState == PlayerTurnEnding
second QueueEmpty callback → BattleState == EnemyTurnEnding
final state after both boundaries = PlayerTurn
```

The existing `Turn.OneFinalQueueEmptyPerTurnBoundary` Automation test was strengthened to assert these observed states instead of only asserting `QueueEmptyCount == 2`.

## Phase 6B Automation gate — 12 TESTS EXPECTED / 2 CONTRACT FAILURES EXPOSED

Prefix:

```text
SlayTheSpireDemo.Phase6B
```

The Phase 6B prefix now contains exactly 12 tests:

```text
Turn.PlayerEndingStateCommitsOnlyAfterEnqueueSuccess
Turn.EnemyBatchInsertionIsAtomic
Turn.LethalEnemyActionSkipsTurnEndedEvent
Turn.OneFinalQueueEmptyPerTurnBoundary
Turn.ResolutionFaultTransitionsBattleState
Turn.TurnEndReactionCompletesBeforeNextTurn
Queue.EmptyBatchIsLegalDuringObserverNotification
Queue.ContinuationOutsideBroadcastRejected
Queue.SecondContinuationRejected
Queue.NonEmptyInsertionRejectedDuringBroadcast
Queue.FaultCancelsDeferredContinuation
Queue.EmptyContinuationRejectedSafely
```

The first six tests and four of the new Queue contract tests are expected to pass on the current source. These two new regressions are intentionally red until production fixes land:

```text
Queue.EmptyBatchIsLegalDuringObserverNotification
Queue.EmptyContinuationRejectedSafely
```

Previously validated on the hardened QueueEmpty source before expanding the suite:

```text
Phase 5   13/13 PASS
Phase 6A  23/23 PASS
Phase 6B   6/6  PASS
Total     42/42 PASS
```

After the two defects are fixed, the owner-only gate must require:

```text
Phase 5   13/13 PASS
Phase 6A  23/23 PASS
Phase 6B  12/12 PASS
Total     48/48 PASS
```

## Manual UE Editor configuration — COMPLETE

The existing Status DataAssets were configured in the UE Editor with one instanced `TurnEndStatusDecayTrigger` each:

```text
DA_Status_Weak
DA_Status_Vulnerable
DA_Status_Frailty
```

Configuration:

```text
Priority       = 0
AmountToRemove = 1
```

No turn-end decay trigger was added to:

```text
DA_Status_Strength
DA_Status_Dexterity
```

## PIE validation — PASSED

### Weak / Vulnerable / Strength cycle

Observed setup:

```text
Enemy  Vulnerable Amount=2
Player Weak       Amount=3
Player Strength   Amount=2
```

Observed player turn ending:

```text
PlayerTurn
→ PlayerTurnEnding
→ TurnEndedAction(Player)
→ TurnEndedEvent(Player)
→ 1 reaction queued
→ Weak Amount 3 - 1 = 2
→ Strength remains Amount=2
→ QueueEmpty
→ EnemyTurn
```

The enemy attack then resolved at Base=5 / Resolved=5, proving the player turn-end reactions had completed before the enemy action began.

Observed enemy turn ending:

```text
Enemy DamageAction
→ TurnEndedAction(Enemy)
→ EnemyTurnEnding
→ TurnEndedEvent(Enemy)
→ 1 reaction queued
→ Vulnerable Amount 2 - 1 = 1
→ QueueEmpty
→ PlayerTurn
```

This validates owner-specific turn-end decay: Player Weak decayed on the player's TurnEnded event, while Enemy Vulnerable did not decay until the enemy's TurnEnded event.

### Frailty / Dexterity cycle

Observed setup:

```text
Player Frailty   Amount=3
Player Dexterity Amount=2
```

The existing Block modifier pipeline still resolved correctly:

```text
Base 5
→ Dexterity: 5 -> 7
→ Frailty:   7 -> 5
→ committed Block=5
```

Observed player turn ending:

```text
TurnEndedEvent(Player)
→ 1 reaction queued
→ Frailty Amount 3 - 1 = 2
→ Dexterity remains Amount=2
→ QueueEmpty
→ EnemyTurn
```

Enemy damage consumed the existing 5 Block, then the enemy TurnEnded event completed with no decay reaction, followed by one final `QueueEmpty` and return to `PlayerTurn`.

### Post-hardening QueueEmpty PIE cycle

After the non-reentrancy change, a clean PIE run used only one `Space` end-turn request. The observed order was:

```text
Player turn started
→ Player turn ending committed
→ TurnEndedEvent(Player), State=PlayerTurnEnding
→ QueueEmpty observed with State=PlayerTurnEnding
→ Enemy turn started
→ DamageAction Base=5 / Resolved=5
→ TurnEndedEvent(Enemy), State=EnemyTurnEnding
→ QueueEmpty observed with State=EnemyTurnEnding
→ Player turn started
```

No `Resolution fault requested` or `Resolution faulted` log occurred.

### Final Phase 6B acceptance

Validated:

```text
Weak / Vulnerable / Frailty decay only on their owner's TurnEnded event
Strength / Dexterity do not decay at turn end
Turn-end reactions finish before the opposing turn begins
Player QueueEmpty observers see PlayerTurnEnding before EnemyTurn starts
Enemy QueueEmpty observers see EnemyTurnEnding before PlayerTurn starts
QueueEmpty broadcasts are not recursively nested by macro turn progression
Normal player → enemy → player flow still works after hardening
No ResolutionFault occurred in the validation cycles
Prior hardened UE5.8 Automation evidence remains 42/42 green
Expanded Queue contract suite requires 48/48 after the two exposed defects are fixed
```

## Next

```text
Phase 6A  COMPLETE / UE5.8 CI PASSED
Phase 6B  GAMEPLAY PASSED / 2 QUEUE CONTRACT FIXES + 48/48 RERUN REQUIRED
Phase 6C  DeckShuffled Event — BLOCKED until expanded Phase 6B gate passes
Phase 6R  Full Regression + deferred test-module extraction
```
