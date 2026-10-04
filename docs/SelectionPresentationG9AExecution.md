# Selection Presentation G9-A — Authority Foundation

Date: **2026-10-05**

Status: **G9-A COMPLETE / VALIDATED / SHADOW ONLY; G9 NOT SEALED**

Design authority: [SelectionPresentationG9Design.md](SelectionPresentationG9Design.md).
G8 baseline: [SelectionPresentationG8FSeal.md](SelectionPresentationG8FSeal.md).
Implementation base HEAD: `15920e7f670308004fcaf497a6e1f2b615dc6dea`.

## Scope and delivered boundaries

The user authorized the next G9 stage. This execution implements G9-A; G9-B production activation remains a separate next stage.

- `ABattleManager` owns `PlayerTurnSerial`. Successful battle setup resets it to zero; each formal `CompletePlayerTurnStart` increments once, skipping zero on unsigned wrap. A read-only `FPlayerTurnAuthorityToken` contains `BattleId + PlayerTurnSerial` and is available only in `PlayerTurn`. Card resolutions/revision changes within that turn do not mint a new token.
- The HUD owns one `FBattleHUDBufferedPlayerInput` value with weak observation bindings and at most one CardSelection or EndTurn intent. It defaults to disabled. Only Automation calls the shadow entry point in G9-A.
- EndTurn intent acceptance and immediate Gameplay query legality are separate results. EndTurn can wait through ordinary `ResolutionBusy` in both PresentationOwned and sessionless DirectBaseline. It is fenced by exact Gameplay turn identity; optional Presentation provenance cannot authorize a different turn.
- Card credentials are captured by the Controller only inside an exact supported Blocking played-card window. They require an already frozen/sealed normal PlayerTurn target, matching player-facing read, legal exact current-Hand object and current Presentation session. No unsealed future revision or busy-queue speculation is accepted.
- Every new card click re-captures the complete credential. Skip, active-envelope recovery and backlog collapse advance a separate card chronology generation; ordinary sequential record completion does not invalidate a waiting click solely because the original visual window ended.
- Accepted EndTurn replaces the older card intent and reports FastInput retirement/transient cancellation as a shadow decision. Rejection does not perform those retirements. An accepted EndTurn excludes later card captures.
- Evaluation drops stale battle/turn/session/binding/mandatory-selection authority. Taking a ready decision clears pending storage before returning it, so a future caller cannot consume it twice.

The owner never invokes Gameplay Request, card selection, normal cancellation, queue advancement or Skip. Production buttons, hover, FastInput, card visuals, `bCanEndTurn` and the G8 ChoosingTarget fallback remain on their existing paths.

## Validation

AUTOMATED GATES:

- UE 5.8 bundled project-file generation and Development Editor build against this worktree's `.uproject`.
- `SlayTheSpireDemo.SelectionPresentation.G9A`: turn identity/restart/ABA, accept-versus-execute separation, DirectBaseline, exact sealed card target/once-only take, unsealed-target rejection and stale fences, accept-before-retire arbitration, mandatory-selection invalidation.
- Affected G8-B and Native FastInput regression suites.
- `git diff --check` and a production-call-site scan proving G9 input capture/evaluation/replay has not been activated.

MANUAL PIE GATES: **none for G9-A**. No production input or visible playback behavior is activated. G9-B requires its own build, focused Automation and manual PIE gate.

### Actual evidence — 2026-10-05

- Bundled UE 5.8 project-file generation: **PASS**, exit 0. Final log: `Saved/Logs/G9AFinalProjectFiles.log`.
- Development Editor Win64 build: **PASS**, exit 0. First clean-worktree build: `Saved/Logs/G9ABuild.log` (158 actions). After configuring the mandatory-selection fixture before battle start and adding post-selection non-resurrection coverage, the affected test-only rebuild also passed: `Saved/Logs/G9AFinalBuild.log`.
- One unattended `UnrealEditor-Cmd` / `-nullrhi` invocation ran these three disjoint suite prefixes:

| Scope | Result |
|---|---|
| `SlayTheSpireDemo.SelectionPresentation.G9A` | **6/6 PASS** |
| `SlayTheSpireDemo.SelectionPresentation.G8B` | **9/9 PASS** |
| `SlayTheSpireDemo.Phase6UIA2N.FastInput` | **2/2 PASS** |

Report: `Saved/AutomationReports/G9A/index.json`; log: `Saved/Logs/G9AAutomation.log`.
The report records `succeeded=17`, `succeededWithWarnings=0`, `failed=0`, `notRun=0`.
The log records `Automation Test Queue Empty 17 tests performed`, with process exit 0.

- `git diff --check`: **PASS**.
- Production-call-site scan: **PASS**. Shadow capture/accept/evaluation/take is invoked only from the Editor test module. The HUD test accessor is guarded by `WITH_DEV_AUTOMATION_TESTS`; the owner defaults disabled and has no Tick, delegate-driven replay or Request/Skip/Cancel call. Existing G8-B target-choice EndTurn rejection passes unchanged.

Startup limitation: before Automation began, the engine reported invalid package summaries for eight existing targeting/interior material assets. Direct inspection confirmed that the targeting arrow and flashlight material files are Git LFS pointer text in this worktree. No asset was modified by G9-A. These deterministic C++ suites passed, but this run establishes no asset-load or visual acceptance; materialize required LFS assets before G9-B manual PIE. No Blueprint, packaged-game or Shipping validation is claimed.

## Forward stop gate

G9-A completion is supported by the build and focused proof for:

1. first formal player turn serial 1; exactly one increment per later formal entry; same-turn revisions preserve identity; old-turn and old-battle tokens cannot ABA-match;
2. EndTurn intent availability differs from immediate execution;
3. DirectBaseline supports EndTurn without a fabricated Presentation session;
4. card buffering requires an already sealed exact target;
5. EndTurn authority is accepted before shadow retirement;
6. shadow processing emits no Gameplay requests and retains sealed G8 production behavior.

The A -> B stop gate is satisfied. G9-B is the next implementation stage; production activation has not started. G9 as a whole remains unsealed.
