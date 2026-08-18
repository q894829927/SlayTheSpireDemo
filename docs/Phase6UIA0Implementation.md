# Phase 6UI-A0 — Playable Gameplay Boundary

Status: **SOURCE IMPLEMENTED / UE5.8 BUILD + AUTOMATION VALIDATION PENDING**.

Phase 6UI-A0 establishes the non-debug gameplay/read boundary required before formal UMG work begins. It does not implement Battle HUD Widgets, Presentation Records, animation playback, Relics or Outcome Preview.

## Scope

Implemented boundary:

```text
authoritative turn / Hand lifecycle
formal Query + Request APIs
shared gameplay-owned validation
AcceptedForResolution request semantics
minimal authoritative Enemy Intent
coherent battle read snapshot / MVVM-style read boundary
```

## Turn and Hand lifecycle

The current first playable rule is explicit and configurable:

```text
OpeningHandDrawCount = 5
PlayerTurnDrawCount  = 5
PlayerTurnEnd        = discard all remaining Hand cards
```

These are initial content rules, not permanent architectural constants.

Opening battle:

```text
BattleStart
↓
initialize combatants / deck / battle scope
↓
commit initial Enemy Intent
↓
opening Draw Action batch
↓
Draw / Shuffle / Trigger gameplay resolution completes
↓
BattleState = PlayerTurn
```

Normal next-player-turn flow:

```text
EnemyTurnEnding QueueEmpty
↓
commit next Enemy Intent
↓
PlayerTurnStarting
↓
restore Energy / clear player Block
↓
turn-start Draw Action batch
↓
Draw / Shuffle / Trigger gameplay resolution completes
↓
PlayerTurn
```

`PlayerTurn` remains the gameplay Request-eligible state. Presentation is still a no-op boundary in UI-A0; no animation completion controls BattleState.

Player end turn:

```text
RequestEndPlayerTurn
↓
shared authoritative validation
↓
[Discard remaining Hand..., TurnEndedAction(Player)]
↓
atomic queue insertion
↓
PlayerTurnEnding
↓
Hand cleanup commits
↓
TurnEndedAction / FTurnEndedEvent
↓
reactions
↓
EnemyTurn
```

Therefore the established UI-A0 `FTurnEndedEvent(Player)` observes the current-version Hand cleanup as already committed. Future Retain/Ethereal/pre-cleanup mechanics must introduce an explicit earlier timing boundary rather than silently changing this event meaning.

## Formal player requests

Gameplay exposes:

```text
QueryCardPlayability(Card)
QueryPlayCard(Card, RequestedTarget)
RequestPlayCard(Card, RequestedTarget)

QueryEndPlayerTurn()
RequestEndPlayerTurn()
```

Query and Request call the same gameplay-owned validator. Query is advisory. Request always revalidates current authoritative state.

Current structured rejection reasons cover the concrete UI-A0 needs:

```text
InvalidBattle
BattleEnded
ResolutionFaulted
WrongTurn
ResolutionBusy
InvalidCard
CardNoLongerInHand
NotEnoughEnergy
InvalidTarget
QueueRejected
```

An accepted Request returns `AcceptedForResolution`. This means the initial authoritative Action/batch was accepted for resolution; it does not claim that all resulting effects/triggers already committed.

`EndPlayerTurn()` remains only as the legacy Blueprint/debug wrapper and forwards into `RequestEndPlayerTurn()`.

## Authoritative target query

`GetLegalTargetsForCard(...)` exposes the current legal target set from gameplay-owned card/turn/energy rules.

Current one-enemy content supports:

```text
Enemy target → authoritative Enemy
Self target  → authoritative Player
None target  → empty target set; Request uses nullptr
```

The formal API is target-set based so future multi-enemy UI does not need to hard-code `BattleManager.Enemy` as its interaction model.

## Enemy Intent

Phase 6UI-A0 introduces the minimum committed Intent model:

```text
None
Attack(BaseAmount)
```

The current fixed intent chooser uses `EnemyTestAttackDamage` only when choosing/committing an Intent. Once committed:

```text
UI/read model reads the committed Intent
↓
EnemyTurn builds its authoritative Action from that same committed Intent
```

Changing the future fixed test amount after the Intent is already committed cannot silently change the currently displayed/executing Intent. The next Intent is committed only after the current EnemyTurn has resolved.

This is intentionally minimal and may later expand to multi-hit, Block, Buff, Debuff, combined, unknown and conditional intents without changing the source-of-truth invariant.

## Coherent read snapshot / MVVM boundary

Phase 6UI-A0 introduces a plain C++ read-model snapshot. It does not require enabling Unreal MVVM tooling.

Conceptually:

```text
Authoritative Gameplay Model
↓
FBattleReadSnapshot
↓
future Battle ViewModel
↓
future UMG View
```

The snapshot is captured only at a safe non-busy gameplay boundary and currently contains:

```text
BattleId
StateRevision
BattleState
Energy / MaxEnergy
Player / Enemy HP, MaxHP, Block, death state
StatusId / Amount / RuntimeSequence copies
committed Enemy Intent
Hand card read views
Discard / Exhaust inspectable card read views
Draw / Hand / Discard / Exhaust / PlayArea counts
```

Card views preserve stable runtime identity through `UCardInstance*` weak identity plus `CardId` / `RuntimeId`; they are not a second authoritative Hand.

`DeckRuntime` therefore exposes only const read-only Hand/Discard/Exhaust collections. Mutable pile containers remain private.

`BattleId` distinguishes battle scope. `StateRevision` advances at meaningful authoritative safe boundaries so UI cache/debug tooling can recognize stale snapshots without using frame time as gameplay identity.

## Automation

New Editor-only Automation source:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA0RegressionTests.cpp
```

Expected UI-A0 tests: 9.

Coverage:

```text
Turn.OpeningHandLifecycle
Turn.HandCleanupBeforeTurnEndedBoundary
Turn.PlayerTurnStartingDrawsAfterEnemy
Request.QueryAndRequestShareTargetValidation
Request.RevalidatesCurrentCardZone
Request.EnergyFailureIsShared
Intent.CommittedIntentDrivesExecution
Snapshot.CoherentRevisionAndCardIdentity
Targets.AuthoritativeLegalTargetSet
```

New trusted owner-only workflow:

```text
.github/workflows/ue-phase6uia0-tests.yml
```

It must build `SlayTheSpireDemoEditor` and preserve all prior gates:

```text
Phase 5       13/13
Phase 6A      23/23
Phase 6B      12/12
Phase 6C       5/5
Phase 6UI-A0   9/9
-------------------
Total         62/62
```

No UE5.8 build/Automation pass is claimed until the self-hosted workflow is actually run successfully.

## Next slice

After UI-A0 passes and documentation is synchronized:

```text
Phase 6UI-A1 — Operable Battle HUD
```

UI-A1 may then bind UMG/ViewModel presentation to the formal Request and read-snapshot boundaries rather than using gameplay-driving debug keys.
