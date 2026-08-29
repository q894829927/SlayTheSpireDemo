# SlayTheSpireDemo Agent Instructions

## Project Goal

This is an Unreal Engine 5.8 C++ learning/demo project inspired by Slay the Spire combat architecture.

Build a small, extensible and deterministic card-battle framework that supports reusable card effects/actions, deck zones, statuses, relic triggers, modifier pipelines and event-driven interactions without hard-coded combinations.

The long-term flow is:

```text
CardData / CardInstance
→ CardEffect
→ BattleAction
→ BattleActionQueue
→ Operation Spec
→ Modifier Pipeline
→ Commit
→ BattleEvent
→ Trigger listeners
→ New BattleAction
→ BattleActionQueue
```

`BattleStateMachine` controls turn flow. `BattleActionQueue` controls execution order. Modifier pipelines handle deterministic pre-commit modification. Events and triggers handle post-commit reactions.

## Current Development Stage

The current active phase is **Phase 6UI-A2E — Unified Blueprint/UMG committed presentation**.

Do not resume unfinished UI-A3 Preview work or begin Phase 7 until UI-A2E is independently complete and sealed, unless the user explicitly changes the order.

Read before current-phase work:

- `docs/UIA2ERemainingSteps.zh-CN.md` for the current implementation order and exact Blueprint/PIE steps.
- `docs/Phase6UIA2EImplementation.md` for the locked A2E contract.
- `docs/Phase6UIA2Implementation.md` for the complete committed-presentation contract.

Project history and current status live in `docs/DevelopmentPhases.md`. Validation evidence lives in `docs/Validation.md`. Architectural background lives in `docs/Architecture.md`.

### Source-of-truth priority

When project documents disagree:

1. Explicit instructions in the current user request.
2. The current phase's dedicated implementation/acceptance document.
3. Directory-specific `AGENTS.md` files.
4. This root `AGENTS.md`.
5. Project-wide summary/history documents.

When editing a subtree, also obey the nearest applicable directory-level `AGENTS.md`. More specific directory rules refine this root contract.

Do not silently choose between contradictory project documents. If a conflict materially affects implementation, identify it before making dependent changes.

## Global Architecture Invariants

### Gameplay authority

Only Gameplay owns authoritative battle, combatant, deck, card, status and turn state.

UI, ViewModel and Presentation may request Gameplay and display frozen state, but must not mutate or duplicate Gameplay truth.

### BattleAction

Authoritative battle-resolution mutation occurs through `BattleAction` and `BattleActionQueue`, unless an existing documented architecture contract explicitly defines another ownership boundary.

Actions may enqueue dependent actions, but must never pump or manually advance the queue. Dependent batches required for one logical chain must be queued before the current action finishes.

### Determinism

The same initial state, input sequence and RNG seed must be reproducible. Correctness and ordering must not depend on frame rate, animation timing, UObject address, actor discovery order, delegate registration order or unstable container iteration.

Use explicit deterministic ordering keys.

### Events and triggers

A `BattleEvent` is an immutable-by-contract fact describing something that already committed.

A Trigger is a read-only rule that decides whether it reacts and builds new `BattleAction` objects. Events and triggers do not directly mutate Gameplay or drive the queue.

### Definitions and runtime state

Shared definitions such as `UCardData`, `UStatusData`, CardEffects, Modifiers and Triggers are immutable runtime configuration. Mutable state belongs to runtime instances and owning containers.

```text
CardData != CardInstance
StatusData != StatusInstance
```

### Generic architecture

Prefer reusable Actions, typed Modifiers, Events, Triggers and data composition. Do not add concrete card/status/relic combination checks or content-specific battle rules.

Do not introduce speculative frameworks, dependencies or future-phase mechanisms without a concrete implemented need. In particular, do not introduce a universal modifier context, a persistent Trigger Registry, premature GAS migration, or model a Relic/Keyword as a Status.

## Gameplay and Presentation

Gameplay and Presentation are separate timelines.

Forbidden:

```text
BattleAction
→ wait for animation
→ next BattleAction
```

Required:

```text
Gameplay commit
→ immutable Presentation Record
→ Gameplay continues independently
```

Presentation may lock UI input for readability, but never blocks Gameplay execution or becomes authoritative state. Presentation failure, timeout, skip, missing callback or disablement must not request a Gameplay `ResolutionFault`.

Directory-specific Presentation and UI contracts live in:

- `Source/SlayTheSpireDemo/Presentation/AGENTS.md`
- `Source/SlayTheSpireDemo/UI/AGENTS.md`

## Development Order and Scope

Respect explicit phase dependencies. Do not skip ahead unless the user explicitly requests it.

Before changing files:

1. Inspect the relevant implementation and documentation.
2. Make the smallest coherent change.
3. Preserve unrelated user changes.
4. Avoid unrelated refactors, public API/asset renames, plugin changes, engine-association changes and build-setting changes.
5. Do not add third-party dependencies without approval.
6. Preserve explainability for learning.

Do not intentionally commit generated or local files:

```text
Binaries/
DerivedDataCache/
Intermediate/
Saved/
.vs/
*.sln
*.slnx
```

## Validation

Never claim:

- successful UE compilation without running the relevant build;
- Automation safety without running the relevant suite;
- Blueprint correctness from C++ Automation alone;
- PIE or packaged-game validation without actually performing it.

When UE Editor work cannot be performed with available tools, label it `USER ACTION REQUIRED` and provide the exact asset, graph/function, nodes, pins, values, compile/save order, expected result and PIE evidence required.

See `docs/Validation.md` and the directory-specific test rules in `Source/SlayTheSpireDemoTests/AGENTS.md`.

## Documentation

`AGENTS.md` files define how work must be performed. The `docs/` directory defines what the project is, what has been implemented and how phases are accepted.

When a meaningful phase changes:

- update the current phase state;
- update acceptance evidence;
- update durable invariants if architecture changed;
- keep status consistent across the relevant documents;
- do not add daily implementation trivia to this root file.

## Multi-Agent Workflow

The primary agent owns:

- architecture;
- phase sequencing;
- cross-module decisions;
- integration;
- conflict resolution;
- final acceptance.

For non-trivial work, use subagents when independent bounded work can be delegated safely.

Good delegation targets include:

- repository exploration;
- dependency and impact analysis;
- test discovery and log analysis;
- bounded non-overlapping implementation;
- independent review.

The primary agent must make architecture decisions before delegating implementation that depends on those decisions.

Parallelize independent investigation and validation work. Do not parallelize implementation slices that have explicit phase or behavioral dependencies.

Never allow two write-capable agents to modify overlapping files or the same behavioral ownership boundary concurrently.

A subagent does not own architecture. If its task requires changing any of the following, it must return the issue to the primary agent instead of redesigning the system independently:

- authoritative state ownership;
- `BattleActionQueue` semantics;
- Modifier/Event/Trigger contracts;
- Gameplay/Presentation boundaries;
- Presentation Record/Envelope semantics;
- phase ordering.

Subagent completion is not acceptance. The primary agent must review the result and required validation evidence before treating the task as complete.
