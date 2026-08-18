# Phase 6B — Battle Turn Wiring

Status: **COMPLETE / PASSED**. Source implementation, UE5.8 Editor build, Automation gate (42/42), manual Status DataAsset configuration, and PIE validation have all passed.

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
→ StartEnemyTurn

StartEnemyTurn
→ atomically enqueue [EnemyDamageAction, TurnEndedAction(Enemy)]
→ commit EnemyTurn + clear enemy Block
→ enemy damage
→ TurnEndedAction enters EnemyTurnEnding
→ TurnEndedEvent(Enemy)
→ reactions
→ one final QueueEmpty for the enemy-ending boundary
→ StartPlayerTurn
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

## Phase 6B Automation gate — PASSED

Prefix:

```text
SlayTheSpireDemo.Phase6B
```

Exactly 6 Phase 6B tests passed:

```text
Turn.PlayerEndingStateCommitsOnlyAfterEnqueueSuccess
Turn.EnemyBatchInsertionIsAtomic
Turn.LethalEnemyActionSkipsTurnEndedEvent
Turn.OneFinalQueueEmptyPerTurnBoundary
Turn.ResolutionFaultTransitionsBattleState
Turn.TurnEndReactionCompletesBeforeNextTurn
```

Validated UE5.8 gates:

```text
Phase 5   13/13 PASS
Phase 6A  23/23 PASS
Phase 6B   6/6  PASS
Total     42/42 PASS
```

The UE5.8 Editor build completed successfully as part of the workflow.

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

### Final PIE acceptance

Validated:

```text
Weak / Vulnerable / Frailty decay only on their owner's TurnEnded event
Strength / Dexterity do not decay at turn end
Turn-end reactions finish before the opposing turn begins
PlayerTurn → PlayerTurnEnding → QueueEmpty → EnemyTurn is correct
EnemyTurn → EnemyTurnEnding → QueueEmpty → PlayerTurn is correct
No ResolutionFault occurred in either validation cycle
Turn progression occurs only from final QueueEmpty boundaries
```

## Next

```text
Phase 6A  COMPLETE
Phase 6B  COMPLETE
Phase 6C  DeckShuffled Event — NEXT
Phase 6R  Full Regression + deferred test-module extraction
```
