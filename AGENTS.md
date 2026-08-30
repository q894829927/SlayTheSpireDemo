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

- `implementation_worker`: primary Blueprint/UMG execution worker for continuous bounded edits;
- `repo_explorer`: on-demand read-only investigation for a concrete unknown only;
- `test_runner`: batch-level build/Automation/log validation;
- `architecture_reviewer`: batch-level independent read-only invariant review.

All project custom subagents use `gpt-5.6-luna`. Reasoning effort is role-specific: implementation/review use higher effort, exploration/testing use lower effort. The project agent concurrency limit is intentionally low to reduce duplicated context and quota consumption.

### Efficiency-first default

For current UI-A2E Blueprint work, the default execution combination is:

```text
Primary Sol
+
implementation_worker
```

Do not automatically start all four Luna agents for every slice.

Normally keep at most `Primary Sol + 1 Luna` active. A second read-only Luna may be used only when there is a concrete independent unknown that would otherwise block the writer.

Never allow two write-capable agents to modify overlapping files or the same behavioral ownership boundary concurrently.

### Agent invocation policy

`implementation_worker` is the default subagent during Blueprint-first implementation. Sol may let it continue across several adjacent edits inside one already-approved functional batch when the contracts and predecessor order are clear. Compile and Save each meaningful Blueprint slice, but do not force a full review/test cycle after every small node group.

`repo_explorer` is not a default predecessor step. Invoke it only for a concrete unknown such as an unclear Blueprint node/function/asset structure, contract ambiguity, ownership question, or specific regression surface. It must not repeat repository baseline investigation or reread facts already captured in `docs/CODEX_GOAL_CHECKPOINT.md`.

`architecture_reviewer` is normally invoked once after a completed functional batch, not after every small Blueprint edit. Review only the final saved batch diff/graph and affected invariants. P0/P1 findings block; P2 findings stay concise and do not block unrelated progress unless they represent a concrete near-term risk.

`test_runner` is normally invoked once after a meaningful functional batch, not after every Blueprint edit. For Blueprint-only slices, Compile/Save and meaningful PIE are the first validation tools. Run focused Automation at batch boundaries when useful. Run Phase6R and Shipping exclusion at final sealing unless Sol explicitly needs an earlier diagnostic run.

### Blueprint-first priority

During UI-A2E implementation, prioritize work in this order:

```text
actual Blueprint / UMG wiring
>
Compile / Save
>
meaningful local PIE at an acceptance boundary
>
continue the next already-unlocked Blueprint slice
>
batch-level architecture review
>
batch-level focused Automation
>
documentation consolidation
```

Do not spend most of a Goal run repeatedly reading documentation, rediscovering the repository baseline, rerunning unchanged Automation, or re-reviewing already accepted paths.

When a contract is already explicit in current phase docs/checkpoint and the saved repository state matches it, Sol should assign the edit boundary directly to `implementation_worker` instead of launching a new explorer pass.

### Batch-oriented UI-A2E execution

Use the documented predecessor order, but group work into functional batches to reduce repeated overhead:

```text
Batch 1 — Status
StatusChanged update/reduction acceptance
→ StatusChanged removal implementation + acceptance

Batch 2 — EndTurn / deck presentation
EnergyChanged
→ remaining CardZoneChanged paths
→ DeckShuffled

Batch 3 — Terminal
Victory
→ Defeat
→ ResolutionFault

Batch 4 — Closure
Global Cancel / Reconcile
→ Scenario A-E
→ final saved Blueprint snapshot
→ final-head Automation / Shipping exclusion
→ documentation closure
→ UI-A2E seal
→ UI-A2 seal
```

Explicit predecessor gates still apply. Do not skip a gate merely to preserve a batch. However, once a predecessor is actually accepted, continue immediately into the next unlocked slice instead of re-running broad investigation/review/test work.

### Validation cadence

Ordinary Blueprint slice:

```text
implement
→ Compile
→ Save
→ smallest useful sanity check
→ continue if the next slice is unlocked
```

Functional batch boundary:

```text
meaningful PIE acceptance
→ one architecture review
→ one focused validation run when useful
→ consolidate validation documentation once
```

Final seal:

```text
final-head focused tests
→ required aggregate regression
→ Shipping exclusion
→ final snapshot/docs
→ seal only with real evidence
```

Subagent completion is not acceptance. Sol remains responsible for deciding whether the real acceptance evidence satisfies the phase contract.

### Resume and checkpoint policy

On Goal resume, default to reading only:

```text
AGENTS.md
docs/CODEX_GOAL_CHECKPOINT.md
git status
current HEAD
current Blueprint / relevant contract section
```

Do not reread the entire detailed implementation manual or repeat the full repository baseline investigation unless the checkpoint is missing, stale, contradictory, or a concrete unknown requires it.

During ordinary node-level work, avoid updating multiple documentation files repeatedly. Update `docs/CODEX_GOAL_CHECKPOINT.md` only when needed for a durable interruption point. Consolidate `UIA2EBlueprintValidationLog.md`, `Validation.md`, and `WBPSavedBlueprintSnapshot.md` at a functional batch acceptance boundary, USER ACTION REQUIRED boundary, or final seal.

If quota/session interruption is approaching:

```text
finish the smallest coherent edit
→ Compile/Save if applicable
→ leave the working tree resumable
→ update CODEX_GOAL_CHECKPOINT.md with exact next action
→ stop before starting another dependent large edit
```

A subagent does not own architecture. If its task requires changing authoritative state ownership, `BattleActionQueue` semantics, Modifier/Event/Trigger contracts, Gameplay/Presentation boundaries, Presentation Record/Envelope semantics, or phase ordering, return the issue to Sol instead of redesigning independently.
