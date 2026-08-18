# Phase 6UI-A0 — Playable Gameplay Boundary

Status: **SOURCE REVIEW FIXES IMPLEMENTED / UE5.8 BUILD + AUTOMATION VALIDATION PENDING**.

Phase 6UI-A0 establishes the non-debug gameplay/read boundary required before formal UMG work begins. It does not implement Battle HUD Widgets, Presentation Records, animation playback, Relics or Outcome Preview.

## Scope

Implemented boundary:

```text
authoritative turn / Hand lifecycle
formal Query + Request APIs
shared gameplay-owned validation
AcceptedForResolution request semantics
minimal authoritative Enemy Intent
current-state gameplay-derived Intent display value
coherent battle read snapshot / MVVM-style read boundary
battle-scoped ReadStateReady notification
battle-RNG initial deck shuffle
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
deterministically shuffle initial DrawPile with battle RNG
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

## Initial deck shuffle

`UDeckRuntime::InitializeFromDefinitions(...)` randomizes the initial DrawPile before the opening Hand is drawn.

The rule is:

```text
create runtime CardInstances in configured definition order
↓
initialize battle-scoped FRandomStream from DeckDebugSeed
↓
initial Fisher-Yates shuffle
↓
Opening Hand draw
```

Initial setup and later gameplay reshuffles reuse one private DrawPile Fisher-Yates helper and the same `FRandomStream`, so initial randomization advances the deterministic battle RNG before later reshuffles.

Event semantics are intentionally different:

```text
Initial DrawPile shuffle during battle setup
→ no DeckShuffled event

DiscardPile -> DrawPile gameplay reshuffle
→ ShuffleDeckAction commit
→ DeckShuffled event
```

Initialization is not counted as a gameplay shuffle trigger. This prevents future shuffle-reactive mechanics such as Sundial from gaining progress merely because battle setup randomized the starting deck.

The initialization-event regression observes the real `BattleManager -> BattleEventDispatcher` path through a test-only dispatcher observation hook. It does not infer “no event” merely because `DeckRuntime` itself lacks a dispatcher.

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

`OnReadStateReady` is deliberately **not re-entrant with a public Request**. Even when every accepted Action resolves synchronously inside `RequestPlayCard` or `RequestEndPlayerTurn`, the stable-read publication is deferred through the CoreTicker. Therefore:

```text
Request...
↓
AcceptedForResolution returns to caller
↓
only later may OnReadStateReady(BattleId, StateRevision) publish
```

A ViewModel may therefore use the natural protocol:

```text
submit Request
↓
if AcceptedForResolution: enter Resolving
↓
wait for OnReadStateReady
↓
refresh coherent snapshot
↓
leave Resolving when the published authoritative state allows it
```

No request-before-lock/revision-race workaround is required or supported as the canonical UI contract.

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

Phase 6UI-A0 keeps two distinct concepts.

### Committed plan

The authoritative committed Intent currently supports:

```text
None
Attack(BaseAmount)
```

The fixed intent chooser uses `EnemyTestAttackDamage` only when choosing/committing an Intent. Once committed:

```text
EnemyTurn builds its authoritative Action from that same committed Intent
```

Changing the future fixed test amount after an Intent is committed cannot silently change the currently committed/executing Intent.

### Gameplay-derived player-facing current value

`TryBuildPlayerFacingReadSnapshot(...)` starts from the coherent authoritative snapshot and, for a committed Attack Intent, builds a read-only `FDamageSpec` using:

```text
Source = Enemy
Target = Player
DamageKind = Attack
BaseAmount = committed Intent BaseAmount
↓
FDamageModifierPipeline::Resolve
↓
EnemyIntentPlayerFacing.CurrentResolvedDamageAmount
```

This deliberately reuses the same Modifier Pipeline as `DamageAction`; Widget/ViewModel code must not reimplement Strength, Weak, Vulnerable or other damage formulas.

The semantic name is important:

```text
CurrentResolvedDamageAmount
= damage amount produced by the current authoritative state at this snapshot revision
```

It is **not**:

```text
guaranteed future EnemyTurn damage
```

A normal counterexample is an expiring player Vulnerable status: the player-turn snapshot can currently resolve Attack 5 to 7, then Player `TurnEnded` reactions can remove Vulnerable before EnemyTurn, allowing the real attack to resolve to 5. A future guaranteed-execution predictor, if required, must model all mandatory pre-execution gameplay transitions rather than hard-code status decay or relabel the current-state value.

The value is therefore described as a **gameplay-derived player-facing value**, not UI authority. Gameplay remains authoritative; UI only presents the coherent snapshot.

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

The raw `TryBuildReadSnapshot(...)` preserves the coherent gameplay state and committed Intent plan. Formal UI/ViewModel consumers should use `TryBuildPlayerFacingReadSnapshot(...)`, which enriches that same snapshot revision with gameplay-derived current Intent display data.

A readable snapshot currently contains:

```text
BattleId
StateRevision
BattleState
Energy / MaxEnergy
Player / Enemy HP, MaxHP, Block, death state
StatusId / Amount / RuntimeSequence copies
committed Enemy Intent
current-state player-facing Intent damage value
Hand card read views
Discard / Exhaust inspectable card read views
Draw / Hand / Discard / Exhaust / PlayArea counts
```

Card views preserve stable runtime identity through `UCardInstance*` weak identity plus `CardId` / `RuntimeId`; they are not a second authoritative Hand.

`DeckRuntime` therefore exposes only const read-only Hand/Discard/Exhaust collections. Mutable pile containers remain private.

`BattleId` distinguishes battle scope. `StateRevision` identifies meaningful authoritative safe boundaries. UI caches must key freshness by the pair `(BattleId, StateRevision)`, not by revision alone and never by frame time.

## ReadStateReady completion boundary

UI-A0 provides a public battle-level stable-read notification:

```text
OnReadStateReady(BattleId, StateRevision)
```

Widgets must not use `BattleActionQueue::OnQueueEmpty` or the Queue-level idle signal directly.

### Healthy resolution path

`BattleActionQueue::OnResolutionIdle` is intentionally later than `OnQueueEmpty`. It may publish only after the complete `PumpQueue()` frame has exited and all of the following are true:

```text
CurrentAction == nullptr
PendingActions.Num() == 0
bIsPumping == false
not inside QueueEmpty broadcast
not executing a post-QueueEmpty continuation
no deferred continuation remains
no resolution fault request
Queue is not faulted
```

A full end-turn flow may therefore broadcast several intermediate `QueueEmpty` boundaries while producing no `OnResolutionIdle`.

BattleManager does not immediately forward `OnResolutionIdle` to UI. It schedules the public read-state publication through the CoreTicker, so the public notification is outside both `PumpQueue()` and the public Request call stack:

```text
final Queue settlement
↓
OnResolutionIdle
↓
BattleManager schedules stable-read publication
↓
public Request returns if this resolution came from a Request
↓
CoreTicker boundary
↓
TryBuildPlayerFacingReadSnapshot
↓
OnReadStateReady(BattleId, StateRevision)
```

### QueueEmpty no-op contract

Healthy empty batches remain legal no-op successes even while QueueEmpty observers are being notified:

```text
empty batch during QueueEmpty
→ accepted no-op

non-empty insertion during QueueEmpty
→ rejected; authoritative progression must use the deferred continuation contract
```

The existing Phase 6B regression `Queue.EmptyBatchIsLegalDuringObserverNotification` remains the guard for this invariant.

### Fault path

A faulted Queue is never treated as healthy idle.

Instead:

```text
Queue enters ResolutionFaulted
↓
existing OnResolutionFaulted listener commits BattleState = ResolutionFaulted
↓
BattleManager schedules stable-read publication
↓
CoreTicker boundary
↓
BattleManager builds readable fault snapshot
↓
OnReadStateReady(BattleId, StateRevision)
```

This guarantees a UI in `Resolving` can still leave that state and display `ResolutionFaulted` rather than waiting forever for healthy idle, while preserving the no-reentrant-public-notification rule.

### Publication deduplication

BattleManager deduplicates by the complete key:

```text
(LastPublishedBattleId, LastPublishedReadStateRevision)
```

A new battle is therefore not suppressed merely because its `StateRevision` happens to equal the final revision number from a prior battle.

## Automation

Editor-only UI-A0 Automation sources cover both the original vertical slice and the review invariants.

Current named invariant coverage includes:

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

Intent.CurrentResolvedDamageUsesDamagePipeline
Intent.ExpiringTargetModifierDoesNotClaimGuaranteedFutureDamage
Deck.InitialShuffleHasExpectedSeededPermutation
Deck.InitialShufflePreservesCardIdentitySet
Deck.InitialShuffleDoesNotEmitDeckShuffled
ReadStateReady.CardResolutionPublishesOnceWhenReadable
ReadStateReady.AsyncActionDoesNotPublishBeforeFinish
ReadStateReady.FullTurnPublishesOnlyAfterMacroFlowStabilizes
ReadStateReady.ResolutionFaultPublishesReadableSnapshot
ReadStateReady.NewBattleIsNotSuppressedByRepeatedRevision
```

`ReadStateReady.CardResolutionPublishesOnceWhenReadable` additionally verifies that a synchronously resolved public card Request returns before its Ready notification can fire.

The trusted owner-only workflow remains:

```text
.github/workflows/ue-phase6uia0-tests.yml
```

The workflow uses exact discovered counts operationally to detect missing tests, but the long-term architecture acceptance rule is **not** a permanent numeric total.

UI-A0 passes only when:

```text
all existing Phase 5 / Phase 6 regression gates pass
+
all currently named UI-A0 invariants pass
+
UE5.8 Editor build passes
```

No UE5.8 build/Automation pass is claimed until the self-hosted workflow is actually run successfully.

## Next slice

After UI-A0 passes and documentation is synchronized:

```text
Phase 6UI-A1 — Operable Battle HUD
```

UI-A1 may then bind UMG/ViewModel presentation to the formal Request, `OnReadStateReady`, and player-facing coherent snapshot boundaries rather than using gameplay-driving debug keys or polling Queue state.
