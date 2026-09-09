# SlayTheSpireDemo Agent Instructions

## Project Goal

This is an Unreal Engine 5.8 C++ learning/demo project inspired by Slay the Spire combat architecture.

Build a small, extensible and deterministic card-battle framework that supports reusable card effects/actions, deck zones, statuses, relic triggers, modifier pipelines and event-driven interactions without hard-coded combinations.

The battle-resolution flow is:

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

Project status and phase dependencies live in `docs/DevelopmentPhases.md`. Architectural background and durable design decisions live in `docs/Architecture.md`. Validation execution policy lives in `docs/ValidationExecutionPolicy.md`; trusted validation evidence lives in `docs/Validation.md`.

Use a dedicated design or implementation document for the goals, migration plan, ordering, fallback strategy and acceptance criteria of a specific initiative. Use `docs/CODEX_GOAL_CHECKPOINT.md` only as the resumable execution state of one Goal, including the current HEAD, completed work, next action and tests already run. A checkpoint records progress; it does not define global rules or override durable project documents.

### Source-of-truth priority

When project documents disagree:

1. Explicit instructions in the current user request.
2. The applicable dedicated design, implementation or acceptance document.
3. Directory-specific `AGENTS.md` files.
4. This root `AGENTS.md`.
5. Project-wide summary/history documents.

Directory rules apply to their subtrees. Consult documents relevant to the changed contract; these links are navigation, not a mandatory reading list for every edit.

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

Gameplay commits immutable Presentation Records and continues independently. Presentation may lock UI input for readability, but never blocks Gameplay or owns its state. Presentation failure, timeout, skip, missing callback or disablement must not request a Gameplay `ResolutionFault`.

Presentation and UI subtrees define their historical-state, playback and input contracts in their own `AGENTS.md` files.

## Retained Legacy Battle UI

The following former Legacy battle UI assets are intentionally retained in the deprecated archive only for historical/reference inspection and explicitly authorized emergency recovery:

```text
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleHUD
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleCard
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleStatus
```

They are permanently deprecated for normal forward development. The Native battle UI is the sole active implementation stack. Follow `docs/LegacyUIPreservationPolicy.md` and the UI-directory `AGENTS.md` rules.

Do not add new production/test runtime references to the retained Legacy assets, restore a Legacy Presenter/default, implement new behavior in Legacy, dual-write Native changes into Legacy, or use Legacy as a new regression-test execution target. Production runtime Legacy HUD/Card/Status dependency count must remain `0`.

R14-B destructive removal is not required under the current project decision and remains not authorized. Do not delete, rename, move, or fix redirectors for those Legacy assets without a separate explicit user request. Likewise, do not restore Legacy runtime fallback for recovery without a new explicit user authorization.

## Development Order and Scope

Respect explicit phase dependencies. Do not skip ahead unless the user explicitly requests it.

Complete the requested implementation, relevant validation and documentation within the authorized scope. Routine local edits, builds and focused tests do not need repeated permission. Resolve routine implementation choices from context; raise material contract conflicts or missing authorization before dependent work.

Preserve unrelated user changes and keep changes coherent and explainable for learning. Avoid unrelated refactors, public API/asset renames, plugin changes, engine-association changes and build-setting changes. New third-party dependencies require approval.

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

Follow `docs/ValidationExecutionPolicy.md` for change-proportional validation, reruns and manual PIE ownership. Documentation-only edits do not require UE builds or Automation. Passing evidence remains valid until affected changes, failures or an explicit rerun requirement invalidate it.

Report only validation actually performed. C++ Automation alone does not prove Blueprint graphs, visual behavior, PIE or packaged-game acceptance. When required manual UE work cannot be performed, label it `USER ACTION REQUIRED` and give the minimal asset/map, actions, expected observations and evidence needed.

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

### Goal checkpoint policy

When work must pause, finish the smallest coherent edit when safe, leave the working tree resumable, and update `docs/CODEX_GOAL_CHECKPOINT.md` with the current HEAD, exact completed state, next action, known blockers and validation already performed. On resume, treat the checkpoint as navigation aid and verify it against the working tree and durable source documents before relying on it.
