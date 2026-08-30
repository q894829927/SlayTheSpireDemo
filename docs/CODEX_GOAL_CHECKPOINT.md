# Codex Goal Checkpoint — Phase 6UI-A2E

Last updated: **2026-08-30 17:20 (Asia/Shanghai)**

## Goal

Continue from the real repository baseline until UI-A2E is `COMPLETE / VALIDATED / SEALED`, then seal UI-A2 only after every required predecessor and final-head gate has current evidence. Do not enter UI-A3 or Phase 7. Do not push.

Goal execution status: **IN PROGRESS — STATUS UPDATE/REDUCTION PIE BLOCKED BY HIDDEN EDITOR**. The StatusChanged update/reduction Router, playback, and Cancel restoration are saved and compiled. Real visual PIE acceptance is still missing, so this slice is not `VALIDATED` and Status removal remains predecessor-locked.

## Current Repository State

```text
Branch: codex/A2E-continue
Current HEAD: 03393e4afa654cd89dbfc7d6043e2f2e2a4f2d05
HEAD subject: docs(ui-a2e): sync checkpoint and validation log to saved StatusChanged update wiring
Working tree before this checkpoint update:
 M Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.uasset
```

Current saved assets:

```text
WBP_BattleHUD.uasset       SHA-256 5CA39898BCF501C24243A704A54B8F92C96AA4FB0DEC59C04C3A24FA3571BD4E
                           length 1,847,067; saved 2026-08-30 17:03:09
WBP_BattleStatus.uasset    SHA-256 205180C8...; unchanged by this bounded edit
Unreal PIE                 stopped (`IsPIERunning=false`)
```

## Last Completed Acceptance Boundary

`StatusChanged creation` remains the latest owner-confirmed Blueprint/PIE acceptance boundary recorded in `docs/UIA2EBlueprintValidationLog.md`.

The current update/reduction slice has reached **WIRED / COMPILED / SAVED / PIE PENDING**, not `VALIDATED`.

## Current Validation Matrix

| Slice / gate | Current evidence |
|---|---|
| CardPlayed | VALIDATED (historical owner PIE evidence) |
| Damage | VALIDATED (historical owner PIE evidence) |
| BlockChanged | VALIDATED (historical owner PIE evidence) |
| CardZoneChanged — PlayArea to destination | VALIDATED (historical owner PIE evidence) |
| StatusChanged creation | VALIDATED (historical owner PIE evidence) |
| StatusChanged update/reduction | WIRED / COMPILED / SAVED on `5CA39898...`; real visual PIE PENDING — USER ACTION REQUIRED |
| StatusChanged removal and later slices | PENDING / predecessor-locked |
| Global Cancel / Reconcile | Status update/reduction Cancel restoration saved; full cross-record acceptance PENDING |
| Scenario A-E final PIE | PENDING |
| A2D5 final-head exactly 6 tests | current working tree focused run 6/6 PASS; not final-head because changes are uncommitted |
| Phase6R final-head 100/100 | PENDING |
| Shipping exclusion final-head | PENDING |

Historical Automation results in `docs/Validation.md` are not final-head evidence for this changed working tree.

## Completed Work

- Read the root and applicable Content, Source, Presentation, UI, and Tests `AGENTS.md` files plus all required UI-A2E/UI-A2 phase, validation, snapshot, and checkpoint documents.
- Verified the merged baseline `main@03393e4` and continued on local branch `codex/A2E-continue` without overwriting unrelated work.
- Used all four project Luna roles for repository exploration, bounded implementation attempt, validation/PIE discovery, and independent architecture review.
- Confirmed the saved update/reduction Router uses exact identity `TargetPresentationId + StatusId + RuntimeSequence`, reuses `ExistingStatusWidget`, applies the frozen StatusView, and returns false on missing identity.
- Rejected the reported `FindStatusWidgetByIdentity` ordering blocker after inspecting actual exec connections: both target-selection setters feed the consuming Branch; `GetChildrenCount(GetTargetStatusWrapBox)` is a pure dependency evaluated from the selected local value. The helper was not edited.
- Reworked `CancelPresentationRecordPlayback` incrementally in the formal HUD asset:
  - preserves timer/card/damage/opacity cleanup;
  - removes the Status `ActiveStatusPresentationWidget -> RemoveFromParent` path;
  - when `ActivePresentationType == StatusChanged` and ViewModel is valid, calls `RebuildStatusIcons(ViewModel.Player.Statuses, WB_PlayerStatuses)` then `RebuildStatusIcons(ViewModel.Enemy.Statuses, WB_EnemyStatuses)`;
  - clears `ActiveStatusPresentationWidget`, type, and token through the common tail;
  - does not Notify and does not re-compare the incoming Cancel token.
- Compiled `WBP_BattleStatus`, then `WBP_BattleHUD`, and saved both assets. The HUD was clean in Editor immediately after save and the on-disk hash became `5CA39898...`.
- Started real PIE on `/Game/SlayTheSpireDemo/Maps/L_BattleTest`. The new run reached `Battle started`, `Player turn is gameplay request-eligible`, and `ReadStateReady ... PresentationAvailable=true` at 09:03:44 UTC, but produced no Status commit.
- Confirmed this Editor session exposes no capturable Slate window and no arbitrary UObject UFUNCTION invocation endpoint. Stopped PIE cleanly instead of claiming visual acceptance.
- A2D5 discovery returned exactly six required test names; test Luna ran all six on the current working tree and obtained 6/6 PASS in 0.108884 s. This is focused regression evidence only, not final-head and not Blueprint/PIE acceptance.

## Changed Files / Assets

```text
Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.uasset
docs/CODEX_GOAL_CHECKPOINT.md
docs/UIA2EBlueprintValidationLog.md
docs/Validation.md
docs/WBPSavedBlueprintSnapshot.md
```

No C++ source, Gameplay authority, Presentation Record/Envelope contract, map, or test was changed.

## Local Commits

Baseline already contains:

```text
ec43e27 feat(ui-a2e): wire StatusChanged update/reduction playback and update snapshot
03393e4 docs(ui-a2e): sync checkpoint and validation log to saved StatusChanged update wiring
```

No new checkpoint commit has yet been created for the `5CA39898...` Cancel restoration edit.

## Latest Architecture Review

- Baseline C++ review found no P0/P1 Gameplay/Presentation separation violation in this slice.
- Existing frozen snapshot, exact token/Cancel boundary, and exact status identity reducers remain intact.
- The reported helper ordering P1 was resolved by actual pin/exec inspection and required no asset change.
- Post-change independent read-only review of saved/loaded `5CA39898...` reports P0=0 and behavior-architecture P1=0. It confirmed Router lifecycle routing, exact identity, update-only `SetStatusView`, Cancel rebuild/no-Notify, and normal Finish Notify-before-clear. The remaining P1 is acceptance-only: missing real Status PIE.
- Known out-of-scope P2 remains: `DrawCardAction.cpp` and `ShuffleDeckAction.cpp` recover `ABattleManager` from `Queue->GetOuter()`. Do not fix it inside this Status slice.

## Latest Build / Automation / PIE Evidence

```text
Blueprint compile — WBP_BattleStatus: actual compile invoked; no compiler error logged
Blueprint compile — WBP_BattleHUD: actual compile at 2026-08-30 09:02:59 UTC; no compiler error logged
Blueprint save: PASS; HUD disk hash changed to 5CA39898... and was clean immediately after save
PIE startup: PASS only for startup/ReadStateReady
Status update/reduction PIE acceptance: NOT RUN / no current-run Status commit / no visual evidence
A2D5 discovery: exactly 6 tests
A2D5 current working tree focused run: 6 passed / 0 failed / 0 skipped; 0.108884 s
A2D5 final-head: PENDING after accepted changes are committed
Phase6R final-head: NOT RUN
Shipping exclusion final-head: NOT RUN
```

The old 08:44 UTC Gameplay logs showing Weak/Vulnerable creation and TurnEnd reduction belong to an earlier PIE run and do not prove the current saved Blueprint playback.

The reviewer observed `is_dirty=true` after graph inspection. Primary Sol immediately called `save_assets` for `WBP_BattleStatus` and `WBP_BattleHUD`, received `true`, then rechecked `WBP_BattleHUD is_dirty=false`. The disk hash remained exactly `5CA39898...`, proving the inspection did not diverge the saved package.

## Unresolved Failures

- The hidden Unreal Editor has no capturable Slate window. MCP can start/stop PIE and read logs, but cannot click the HUD or verify that update/reduction preserves a single exact-identity widget without flashback.
- MCP exposes no arbitrary UObject UFUNCTION invocation endpoint for `ABattleManager::TestApplyPhase5AStatuses()`; this run therefore could not produce the deterministic creation + same-sequence reapply scenario.
- The implementation Luna's two whole-graph DSL attempts failed (`Could not connect pin ViewModel to self`; one also had an unclosed parenthesis). Neither attempt saved the asset. Do not repeat whole-graph DSL rewriting for this complex HUD.
- Status removal, EnergyChanged, remaining CardZoneChanged paths, shuffle, terminal, full Cancel/Reconcile, Scenario A-E, final snapshot, final-head Automation, and both Seals remain pending.

## USER ACTION REQUIRED

Use a visible Unreal Editor with the saved asset `/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD` at hash `5CA39898...`. Do not edit Status removal or later slices.

1. Open `WBP_BattleHUD`, function/event `CancelPresentationRecordPlayback`, and confirm the saved Status path exactly:
   - condition: `ActivePresentationType == StatusChanged`;
   - valid object: `ViewModel`;
   - call 1: `RebuildStatusIcons`, `Statuses = ViewModel.Player.Statuses`, `TargetWrapBox = WB_PlayerStatuses`;
   - call 2: `RebuildStatusIcons`, `Statuses = ViewModel.Enemy.Statuses`, `TargetWrapBox = WB_EnemyStatuses`;
   - then `ActiveStatusPresentationWidget = None`, `ActivePresentationType = None`, `ActivePresentationToken = None` through the common cleanup tail;
   - no `RemoveFromParent` on the Status path, no `NotifyPresentationFinished`, and no second comparison against the incoming `Token`.
2. Compile/save in order: `WBP_BattleStatus` (0 errors, Save), then `WBP_BattleHUD` (0 errors, Save). Reopen the HUD once and confirm the graph persists.
3. PIE `/Game/SlayTheSpireDemo/Maps/L_BattleTest` and capture screenshots/video plus Output Log for:
   - creation regression;
   - reapply/increase with `bCreated=false`, `bRemoved=false`, same `TargetPresentationId + StatusId + RuntimeSequence`, exactly one widget, frozen `AmountAfter`, no duplicate/removal/A→B→A flashback;
   - non-removing TurnEnd reduction with `AmountAfter > 0`, same widget, no duplicate/removal/flashback;
   - a following record completes and Controller returns Idle/not Resolving.
4. Preferred deterministic Gameplay trigger while the queue is idle in PlayerTurn: invoke `ABattleManager::TestApplyPhase5AStatuses()` once. Expected logs include `Strength#N Amount=2 Created=true`, then the same `Strength#N Amount=3 Created=false`, plus Enemy `Weak#N Amount=2 Created=true`. End Turn should then log `ReduceStatusAction committed for Weak#N Amount 2 -> 1 Reason=3`.

Do not report PIE PASS from logs alone; the one-widget/no-flashback UI observation is required.

## Next Exact Action

When visible-Editor evidence is available, or the user replies `继续` from an environment with a visible Editor:

1. Verify HEAD, git status, and the saved HUD hash; inspect the real saved graph.
2. Run/collect Creation, Increase/Reapply, and non-removing Reduction PIE evidence above.
3. Record the final architecture review result and focused validation result.
4. Only if every item passes, mark `StatusChanged update/reduction = VALIDATED` and advance the edit boundary to StatusChanged removal.
5. If evidence is still unavailable, keep the predecessor gate pending and do not repeat repository-baseline investigation.

## Next Allowed Edit Boundary

```text
Asset: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD
Behavior: StatusChanged update/reduction acceptance only
Allowed edit if a defect is proven: Router/playback/helper/Cancel restoration for this same slice
Forbidden: Status removal, EnergyChanged, remaining zone paths, shuffle, terminal, A3
```

## Remaining Ordered Slices

```text
StatusChanged update/reduction PIE acceptance
StatusChanged removal acceptance
EnergyChanged acceptance
CardZoneChanged remaining paths
DeckShuffled
Victory / Defeat / ResolutionFault
global Cancel / Reconcile
Scenario A-E full PIE
final saved Blueprint snapshot
final-head Automation and Shipping exclusion
documentation closure
UI-A2E seal
UI-A2 seal
```

## Do Not Repeat

- Do not treat saved/compiled update/reduction wiring as `VALIDATED` without real visual PIE.
- Do not treat `UIA2EDetailedImplementationSteps.zh-CN.md` as completion evidence.
- Do not implement Status removal or later slices before this predecessor gate passes.
- Do not treat historical Automation or old PIE logs as final-head/current-asset evidence.
- Do not repeat whole-graph DSL rewriting of `WBP_BattleHUD`.
- Do not redesign `FindStatusWidgetByIdentity`; actual exec/pure dependency order has already been inspected.
- Do not enter UI-A3 or Phase 7 and do not push.
