# Phase 6B — Battle Turn Wiring

Status: **implemented in source; UE5.8 build/Automation/PIE validation pending**.

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

`ABattleManager` now creates and owns one transient `UBattleEventDispatcher` for the battle and subscribes to `UBattleActionQueue::OnResolutionFaulted`.

`UTurnEndedAction` is a production BattleAction. It does not directly start the next turn. It coordinates the post-commit event-emission point and always lets final `QueueEmpty` own macro turn progression.

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

## Phase 6B Automation gate

New prefix:

```text
SlayTheSpireDemo.Phase6B
```

Exactly 6 tests are expected:

```text
Turn.PlayerEndingStateCommitsOnlyAfterEnqueueSuccess
Turn.EnemyBatchInsertionIsAtomic
Turn.LethalEnemyActionSkipsTurnEndedEvent
Turn.OneFinalQueueEmptyPerTurnBoundary
Turn.ResolutionFaultTransitionsBattleState
Turn.TurnEndReactionCompletesBeforeNextTurn
```

The execution-order timing test gives the player a one-point Vulnerable status that both modifies incoming Attack damage and decays on the player's `TurnEndedEvent`.

Expected behavior:

```text
Player Vulnerable Amount=1
→ player TurnEnded reaction removes Vulnerable
→ player-ending QueueEmpty
→ enemy Attack Base=5
→ Vulnerable is already absent
→ resolved damage remains 5, not 7
```

This directly proves turn-end reactions complete before the next turn's actions begin.

Owner-only workflow:

```text
.github/workflows/ue-phase6b-tests.yml
```

Expected gates:

```text
Phase 5   13
Phase 6A  23
Phase 6B   6
Total     42
```

Do not mark Phase 6B COMPLETE until the UE5.8 Editor build succeeds and all 42 tests pass.

## Manual UE Editor configuration after source validation

Do not text-edit `.uasset` files.

After the source/Automation gate passes, configure these existing Status DataAssets in the UE Editor:

```text
DA_Status_Weak
DA_Status_Vulnerable
DA_Status_Frailty
```

For each, add one instanced Trigger:

```text
Class          = TurnEndStatusDecayTrigger
Priority       = 0
AmountToRemove = 1
```

Do not add turn-end decay to:

```text
DA_Status_Strength
DA_Status_Dexterity
```

Then PIE-validate that Weak/Vulnerable/Frailty decay only on their owner's turn end, their reactions complete before the opposing turn begins, and Strength/Dexterity remain unchanged.
