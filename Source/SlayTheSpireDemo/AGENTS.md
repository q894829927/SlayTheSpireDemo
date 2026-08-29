# Runtime Module Rules

Applies to `Source/SlayTheSpireDemo/**`.

Read `docs/Architecture.md` before cross-system architecture changes. Presentation and UI subdirectories add stricter local rules through their own `AGENTS.md` files.

## Runtime Authority

This module contains authoritative Gameplay and non-authoritative Presentation/UI runtime code. It must not depend on `SlayTheSpireDemoTests`.

`ABattleManager` may temporarily own battle orchestration, battle-scoped allocators and PIE debug entry points. Do not split it for aesthetic reasons or keep adding permanent rule-test entry points when Automation can cover the invariant.

## BattleAction and Queue

Authoritative battle-resolution mutation occurs through `BattleAction` and `BattleActionQueue`, unless an existing documented architecture contract explicitly defines another ownership boundary.

Each action must:

- capture stable intent at enqueue-time and resolve mutable-state-dependent values at Execute-time;
- validate action-specific execution preconditions;
- fail soft and log when useful;
- always honor the required `Finish()`/return contract;
- never call `PumpQueue`, `ProcessNext` or equivalent queue advancement.

Dependent batches required for one logical chain must be queued before the current action finishes. Turn-state transitions that depend on queued work are transactional: build and validate the required Action/batch, atomically enqueue it, commit the associated state and only then start processing.

Resolution budget is checked before dequeuing the next action. A Queue resolution fault broadcasts once, suppresses normal `QueueEmpty` and rejects further queue mutation. `QueueEmpty` is a non-reentrant observable boundary and must not depend on multicast registration order.

## Definitions and Runtime Instances

`UCardData`, `UStatusData` and their instanced effect/modifier/trigger subobjects are immutable shared definitions. Runtime state belongs to `UCardInstance`, `UStatusInstance` and authoritative containers. The Relic definition/runtime ownership model is not locked until Phase 7 introduces a concrete requirement.

Card effects capture base intent and build reusable actions; they do not control the queue or own unrelated battle rules. Card destination is resolved at cleanup Execute-time and delegated to `UDeckRuntime`.

Status reapplication preserves its `RuntimeSequence`; removal followed by recreation receives a new sequence. `ReduceStatusAction` targets an exact runtime Status instance.

## Modifiers

Use typed operation specs and pipelines. Within a typed domain, deterministic ordering is:

```text
Phase → Priority → RuntimeSequence → LocalModifierIndex
```

Ratio modifiers use explicit non-negative numerators and positive denominators, safe integer intermediates and floor after each modifier.

Do not introduce a universal modifier context, generic contributor framework or GameplayTag-based damage taxonomy without a concrete implemented need. While Status is the only real source, direct StatusContainer collection remains acceptable; extract the smallest multi-source boundary when Relics arrive.

## Events and Triggers

Commit precedes event dispatch. RuntimeSource is authoritative for Trigger Owner, RuntimeSequence and identity.

Event/Trigger context references exist only for synchronous dispatch and must never be cached. Trigger definitions are read-only rule builders; they build Actions instead of directly mutating Gameplay or driving the queue.

Trigger eligibility uses snapshot semantics while resulting Actions validate live state at Execute-time. Ordering is `Priority → RuntimeSequence → LocalTriggerIndex`. Per-trigger build failure is fail-soft; final framework Reaction-batch insertion failure requests Queue `ResolutionFault`.

Do not add `TriggerPhase` or a persistent Trigger Registry before a real mechanic/source requires it.

`FDeckShuffledEvent` emits only after a successful discard-to-draw shuffle commit. Reactions precede RetryDraw. Initial battle setup shuffle consumes battle RNG but emits no Gameplay or Presentation shuffle record; opening-hand setup draws likewise emit no Presentation draw records.

## Public Gameplay Boundary

Formal UI consumers use battle-level Query/Request and Ready/coherent-snapshot APIs, never QueueEmpty as their completion protocol. Query results are advisory; Request revalidates current authoritative state.

Neither `OnReadStateReady` nor `OnPresentationResolutionReady` may fire re-entrantly before an accepted public Request returns.

`EnemyIntentPlayerFacing.CurrentResolvedDamageAmount` is a current-state resolved value, not guaranteed future EnemyTurn damage.

## Presentation Dependency Boundary

`ACombatant`, `UDeckRuntime` and `UStatusContainer` return typed Commit/Mutation results and do not depend on the Presentation Recorder.

RecordWriter/Sink is optional, explicit and battle-scoped. Actions must not discover it by world/actor search, UObject Outer, global state or singleton. Nested/reaction Actions in one active Resolution receive the same writer through explicit context propagation.

Gameplay Commit, Action Finish and queue ordering remain valid if Presentation is absent or fails.
