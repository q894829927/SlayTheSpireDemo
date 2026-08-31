# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-08-31 12:18 (Asia/Shanghai)**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

Goal execution status: **IN PROGRESS — R0 COMPLETE / VALIDATED; R1 NOT STARTED**.

## Current Repository State

```text
Branch: main
R0 starting HEAD: 4e977f3af3980d7d534867d737a6b78539c92314
Current R0 implementation HEAD: 12e2f76f456b70c9c9f811a41604c61ef5168b36
HEAD subject: docs(ui-a2n): 完成 R0 基线与注入决策
Production map: /Game/SlayTheSpireDemo/Maps/L_BattleTest
Production WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD_C
PIE: stopped after the R0 Legacy Scenario A smoke
Native classes/assets/test map: not created
R1: not started
```

## Completed R0 Boundary

- Created `docs/UIA2NNativeHUDBaseline.md` with the sealed evidence map and the
  visible/validation/Finish/Cancel/invalid/exact-token contract for every Record.
- UE5.8 UMGToolSet read-only export confirmed exactly 75 HUD Designer Widgets,
  including real type and `IsVariable` values: 33 true, 42 false.
- Native classification is complete: 23 Required BindWidget, 6
  BindWidgetOptional, 46 Designer-only.
- `Txt_DamagePresentation` is a `UMG.TextBlock` and the current disk asset has
  `IsVariable=true`. This corrects the earlier false assumption; no Legacy edit is
  required before the future Native duplicate binds it.
- The single injection point remains `ABattleHUDPresenter::WidgetClass`.
- The non-production strategy is locked: R2 will create `L_BattleTest_Native` and
  override only that map's Presenter instance to `WBP_BattleHUD_Native`.
- Production `L_BattleTest`, `BP_BattleHUDPresenter`, and `DefaultEngine.ini`
  remain on `WBP_BattleHUD`. No runtime Legacy/Native toggle or second Controller
  assembly path was added.

## R0 Validation Evidence

Legacy asset hashes after the documentation work and PIE are unchanged:

```text
WBP_BattleHUD
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

The UE5.8 floating PIE smoke used the formal production map and UI interaction:

```text
runtime Presenter widgetInstance: WBP_BattleHUD_C_0
initial: Energy 5/5, Enemy 100/100, Hand 5, Discard 0
request: click Strike#6, then click the Enemy presentation
active visual: one PlayArea Strike transient; no duplicate
final: Energy 4/5, Enemy 94/100, Hand 4, Discard 1
ViewModel: stateRevision=5, interactionState=Idle, bInputLocked=false, outcome=None
HUD cleanup: Active Type=None, default Token, empty timer, no transient references
PIE stopped; HUD/Card/Status assets not dirty
```

R0 intentionally did not rerun A2D5, Phase6R, or Shipping. Their sealed evidence
is referenced from the Native HUD baseline and was not replaced by this smoke.

## Next Exact Action — R1 Only

Implement the backward-compatible Native ViewModel-changed extension hook described
in R1 of `docs/Phase6UIA2NNativeHUDRefactor.md`:

```text
UBattleHUDWidgetBase::HandleViewModelChanged
→ preserve the existing CancelTrackedPresentationPlayback suppression semantics
→ call NativeOnBattleHUDViewModelChanged

base default NativeOnBattleHUDViewModelChanged
→ BP_OnViewModelChanged
```

Regenerate project files, run the Editor build, and prove the Legacy WBP still
receives its existing Blueprint refresh while the existing cancellation suppression
contract remains intact. Do not create `UBattleHUDWidget`, `WBP_BattleHUD_Native`,
`L_BattleTest_Native`, Native Card/Status Widgets, or any Record playback
implementation until their ordered phases.

## Blockers

None. R0 is complete and the repository is recoverable at the R0 implementation
HEAD above. Do not push.
