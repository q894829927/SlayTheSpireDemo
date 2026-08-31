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

## Documentation Sources and Priority

Project status and phase dependencies live in `docs/DevelopmentPhases.md`. Architectural background and durable design decisions live in `docs/Architecture.md`. Validation evidence lives in `docs/Validation.md`.

Use a dedicated design or implementation document for the goals, migration plan, ordering, fallback strategy and acceptance criteria of a specific initiative. Use `docs/CODEX_GOAL_CHECKPOINT.md` only as the resumable execution state of one Goal, including the current HEAD, completed work, next action and tests already run. A checkpoint records progress; it does not define global rules or override durable project documents.

### Source-of-truth priority

When project documents disagree:

1. Explicit instructions in the current user request.
2. The applicable dedicated design, implementation or acceptance document.
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

### UE 5.8 PowerShell Project and Build Commands

For this project, use the bundled UE 5.8 .NET runtime and the following PowerShell commands. The standard compile workflow is to run the project-file generation command first, then the editor build command.

1. Regenerate Visual Studio project files:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" "E:\Unreal engine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" -ProjectFiles -project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" -game -engine -2022
```

2. Compile the editor target:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlayTheSpireDemoEditor Win64 Development -Project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" -WaitMutex -FromMsBuild -2022 -architecture=x64
```

The first command regenerates `.sln`/`.slnx` files; it does not compile the project. The second command is the command equivalent to Visual Studio's project `生成(U)` for the `Development Editor | Win64` configuration. Do not replace these with a system `dotnet` invocation; UE 5.8 requires the bundled .NET 10 runtime.

## Documentation

`AGENTS.md` files define how work must be performed. The `docs/` directory defines what the project is, what has been implemented and how phases are accepted.

When a meaningful phase changes:

- update the current phase state;
- update acceptance evidence;
- update durable invariants if architecture changed;
- keep status consistent across the relevant documents;
- do not add daily implementation trivia to this root file.

## Multi-Agent Workflow

The primary Sol agent remains the sole owner of:

- architecture;
- phase sequencing and predecessor gates;
- edit boundaries;
- cross-module decisions;
- integration and conflict resolution;
- acceptance claims and final sealing.

Project-scoped custom agents are defined under `.codex/agents/`:

- `implementation_worker`: primary bounded implementation worker for an already-approved scope. It may modify C++, Blueprint/UMG, tests or documentation only within the explicit edit boundary assigned by Sol;
- `repo_explorer`: read-only investigator for a concrete codebase, asset, contract or regression-surface unknown;
- `test_runner`: build, Automation, log and regression validation worker for an explicitly assigned validation scope;
- `architecture_reviewer`: independent read-only reviewer of affected architectural invariants and regression risk.

All project custom subagents use `gpt-5.6-luna`. Reasoning effort is role-specific: implementation/review use higher effort, exploration/testing use lower effort. The project agent concurrency limit is intentionally low to reduce duplicated context and quota consumption.

### Agent invocation policy

Use subagents only when the work has a concrete bounded scope. Do not automatically start every available role, and do not use an explorer to repeat repository facts already established by current documents or a trustworthy checkpoint.

Sol must assign each implementation worker an explicit ownership boundary covering files, assets or behavior. Never allow two write-capable agents to modify overlapping files or the same behavioral ownership boundary concurrently. Read-only investigation and review may run in parallel only when they have distinct, useful questions.

An implementation worker may continue across adjacent edits while the approved boundary and contracts remain unchanged. It must return architectural ambiguity, cross-module ownership changes or requests outside that boundary to Sol rather than expanding scope independently.

Use `repo_explorer` only when a concrete unknown would materially affect implementation or review. Use `architecture_reviewer` after a meaningful coherent change set or when architectural risk warrants independent review. Use `test_runner` for a meaningful validation scope rather than repeatedly rerunning unchanged checks after every small edit.

Subagent completion is not acceptance. Sol must inspect and integrate the result, resolve conflicts, and decide whether the required build, Automation, Blueprint compile/save, PIE, packaged-game or other acceptance evidence actually satisfies the applicable contract.

A subagent does not own architecture. If its task requires changing authoritative state ownership, `BattleActionQueue` semantics, Modifier/Event/Trigger contracts, Gameplay/Presentation boundaries, Presentation Record/Envelope semantics or phase ordering, it must return the issue to Sol instead of redesigning independently.

### Goal checkpoint policy

When work must pause, finish the smallest coherent edit when safe, leave the working tree resumable, and update `docs/CODEX_GOAL_CHECKPOINT.md` with the current HEAD, exact completed state, next action, known blockers and validation already performed. On resume, treat the checkpoint as navigation aid and verify it against the working tree and durable source documents before relying on it.
