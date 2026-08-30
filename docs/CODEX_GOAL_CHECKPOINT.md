# Codex Goal Checkpoint — Phase 6UI-A2E

Last updated: **2026-08-30 22:00 (Asia/Shanghai)**

## Goal

Continue from the real repository baseline until UI-A2E is `COMPLETE / VALIDATED / SEALED`, then seal UI-A2 only after every required predecessor and final-head gate has current evidence. Do not enter UI-A3 or Phase 7. Do not push.

Goal execution status: **IN PROGRESS — STATUS PIE ACCEPTANCE BLOCKED BY NO CAPTURABLE HUD VIEW**. The StatusChanged update/reduction and removal Router/playback paths, plus Cancel restoration, are saved; the HUD was recompiled and saved after removing a redundant `ExistingStatusWidget → SetStatusView.self` data link. Real visual PIE acceptance is still missing, so both StatusChanged slices remain `WIRED / COMPILED / SAVED / PIE PENDING`, not `VALIDATED`.

## Current Repository State

```text
Branch: main
Current HEAD: 47b09108f9b2c12b032ee8bce2dee5089f21e6a2
HEAD subject: chore(agents): adopt efficient batch-oriented multi-agent workflow
Working tree after this asset/document synchronization:
 M Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard.uasset
 M Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.uasset
 M docs/CODEX_GOAL_CHECKPOINT.md
 M docs/UIA2EBlueprintValidationLog.md
 M docs/UIA2ERemainingSteps.zh-CN.md
 M docs/WBPSavedBlueprintSnapshot.md
```

Current saved assets:

```text
WBP_BattleHUD.uasset       SHA-256 574FF05882D4876831B373D60D23CA3DFF564AE19E4AD143B77CE4724E48EAA3
                           length 1,874,310; saved 2026-08-30 21:50:03
WBP_BattleCard.uasset      SHA-256 1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F
                           length 150,278; saved 2026-08-30 19:55:27
WBP_BattleStatus.uasset    SHA-256 205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
                           length 116,261; saved 2026-08-30 16:38:23
Unreal PIE                 stopped (`IsPIERunning=false`)
```

## Last Completed Acceptance Boundary

`StatusChanged creation` remains the latest owner-confirmed Blueprint/PIE acceptance boundary recorded in `docs/UIA2EBlueprintValidationLog.md`.

The current update/reduction and removal slices have reached **WIRED / COMPILED / SAVED / PIE PENDING**, not `VALIDATED`.

## Current Validation Matrix

| Slice / gate | Current evidence |
|---|---|
| CardPlayed | VALIDATED (historical owner PIE evidence) |
| Damage | VALIDATED (historical owner PIE evidence) |
| BlockChanged | VALIDATED (historical owner PIE evidence) |
| CardZoneChanged — PlayArea to destination | VALIDATED (historical owner PIE evidence) |
| StatusChanged creation | VALIDATED (historical owner PIE evidence) |
| StatusChanged update/reduction | WIRED / COMPILED / SAVED on `574FF058...`; real visual PIE PENDING — USER ACTION REQUIRED |
| StatusChanged removal | WIRED / COMPILED / SAVED on `574FF058...`; real visual PIE PENDING — predecessor acceptance-locked |
| Later A2E slices | PENDING / predecessor-locked |
| Global Cancel / Reconcile | Status update/reduction Cancel restoration saved; full cross-record acceptance PENDING |
| Scenario A-E final PIE | PENDING |
| A2D5 final-head exactly 6 tests | current working tree focused run 6/6 PASS; not final-head because changes are uncommitted |
| Phase6R final-head 100/100 | PENDING |
| Shipping exclusion final-head | PENDING |

Historical Automation results in `docs/Validation.md` are not final-head evidence for this changed working tree.

## Completed Work

- Confirmed the current saved HUD Router uses exact identity `TargetPresentationId + StatusId + RuntimeSequence`, reuses `ExistingStatusWidget`, applies the frozen StatusView, and returns false on missing identity.
- Confirmed the saved `CancelPresentationRecordPlayback` Status path rebuilds Player then Enemy historical status lists, clears presentation-only references through the common tail, does not Notify, and does not remove the formal status widget directly.
- In the current `WBP_BattleHUD` EventGraph, removed the redundant direct `ExistingStatusWidget → SetStatusView.self` connection and retained the `ActiveStatusPresentationWidget` output as the single update input.
- Re-read the edited pins through the UE Blueprint interface: `SetStatusView.self` now has exactly one incoming data connection; the separate `ExistingStatusWidget` output remains connected to its reroute and the removal visibility path.
- Compiled `WBP_BattleHUD` through the UE Blueprint interface and saved it through AssetTools. The asset is clean (`is_dirty=false`) and the current on-disk hash is `574FF058...`.
- The current turn did not run PIE; `IsPIERunning=false`. Therefore StatusChanged update/reduction and removal remain saved implementation facts, not visual acceptance.

## Changed Files / Assets

```text
Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.uasset
Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard.uasset
docs/CODEX_GOAL_CHECKPOINT.md
docs/UIA2EBlueprintValidationLog.md
docs/UIA2ERemainingSteps.zh-CN.md
docs/WBPSavedBlueprintSnapshot.md
```

No C++ source, Gameplay authority, Presentation Record/Envelope contract, map, or test was changed.

## Local Commits

Current HEAD is `main@47b0910` (`chore(agents): adopt efficient batch-oriented multi-agent workflow`).
The earlier A2E implementation/checkpoint commits remain available on the historical
`codex/A2E-continue` branch; this turn created no commit and intentionally leaves the
asset/document synchronization in the working tree.

## Latest Architecture Review

- Baseline C++ review found no P0/P1 Gameplay/Presentation separation violation in this slice.
- Existing frozen snapshot, exact token/Cancel boundary, and exact status identity reducers remain intact.
- The reported helper ordering P1 was resolved by actual pin/exec inspection and required no asset change.
- The earlier independent read-only review reports P0=0 and behavior-architecture P1=0. It confirmed Router lifecycle routing, exact identity, update-only `SetStatusView`, Cancel rebuild/no-Notify, and normal Finish Notify-before-clear. This turn only removed a redundant data link and did not change Gameplay/Presentation ownership; the remaining issue is acceptance-only: missing real Status PIE.
- Known out-of-scope P2 remains: `DrawCardAction.cpp` and `ShuffleDeckAction.cpp` recover `ABattleManager` from `Queue->GetOuter()`. Do not fix it inside this Status slice.

## Latest Build / Automation / PIE Evidence

```text
Blueprint compile — WBP_BattleHUD: UE Blueprint tool returned successfully after duplicate-link cleanup
Blueprint save: PASS; AssetTools returned true; HUD is_dirty=false
HUD disk state: SHA-256 574FF05882D4876831B373D60D23CA3DFF564AE19E4AD143B77CE4724E48EAA3; saved 2026-08-30 21:50:03 Asia/Shanghai
PIE: not run in this asset/document synchronization; IsPIERunning=false
Status update/reduction and removal PIE acceptance: PENDING / no current visual evidence
A2D5 discovery: exactly 6 tests
A2D5 current working tree focused run: 6 passed / 0 failed / 0 skipped; 0.108884 s
A2D5 final-head: PENDING after accepted changes are committed
Phase6R final-head: NOT RUN
Shipping exclusion final-head: NOT RUN
```

The old Gameplay logs showing Weak/Vulnerable creation and TurnEnd reduction belong to an earlier PIE run and do not prove the current saved Blueprint playback. The current saved graph was re-read after the edit, and the single `SetStatusView.self` data source plus the current HUD hash were confirmed.

## Unresolved Failures

- The hidden Unreal Editor has no capturable Slate window. MCP can start/stop PIE and read logs, but cannot click the HUD or verify that update/reduction preserves a single exact-identity widget without flashback.
- MCP exposes no arbitrary UObject UFUNCTION invocation endpoint for `ABattleManager::TestApplyPhase5AStatuses()`; this run therefore could not produce the deterministic creation + same-sequence reapply scenario.
- The implementation Luna's two whole-graph DSL attempts failed (`Could not connect pin ViewModel to self`; one also had an unclosed parenthesis). Neither attempt saved the asset. Do not repeat whole-graph DSL rewriting for this complex HUD.
- Real visual acceptance for Status update/reduction and removal, EnergyChanged, remaining CardZoneChanged paths, shuffle, terminal, full Cancel/Reconcile, Scenario A-E, final-head Automation, and both Seals remain pending. The Status removal implementation itself is saved; it is no longer an unwired implementation task.

## USER ACTION REQUIRED

Use a visible Unreal Editor with the saved asset `/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD` at hash `574FF058...` for the remaining PIE acceptance. Do not add Energy, shuffle, terminal, or other later Record routes before the Status predecessor gates pass.

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
2. Run/collect Creation, Increase/Reapply, non-removing Reduction, and Removal PIE evidence above.
3. Record the final architecture review result and focused validation result.
4. Only if every StatusChanged scenario passes, mark update/reduction and removal as `VALIDATED`.
5. If evidence is still unavailable, keep the Status predecessor gate pending and do not repeat repository-baseline investigation.

## Next Allowed Edit Boundary

```text
Asset: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD
Behavior: StatusChanged update/reduction and removal PIE acceptance
Allowed edit if a defect is proven: Router/playback/helper/Cancel restoration for this same slice
Forbidden: new later Record routes, EnergyChanged, remaining zone paths, shuffle, terminal, A3
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
- Do not add later Record slices before StatusChanged update/reduction and removal acceptance pass.
- Do not treat historical Automation or old PIE logs as final-head/current-asset evidence.
- Do not repeat whole-graph DSL rewriting of `WBP_BattleHUD`.
- Do not redesign `FindStatusWidgetByIdentity`; actual exec/pure dependency order has already been inspected.
- Do not enter UI-A3 or Phase 7 and do not push.
