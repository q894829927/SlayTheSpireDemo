# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-08-31 12:52 (Asia/Shanghai)**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

Goal execution status: **IN PROGRESS — R0 COMPLETE / VALIDATED; R1 SOURCE IMPLEMENTED / UE VALIDATION PENDING**.

## Current Repository State

```text
Base branch: main
R0 starting HEAD: 4e977f3af3980d7d534867d737a6b78539c92314
R0 checkpoint HEAD: de30f278b405f2cab6f96fb4e88a84acc53cfd49
R1 working branch: a2n/r1-native-hook
R1 source implementation commit: 496224de8fa549e7ac3563adf04e58743f072b85
R1 source subject: refactor(ui-a2n): add native HUD refresh hook
Production map: /Game/SlayTheSpireDemo/Maps/L_BattleTest
Production WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD_C
Native classes/assets/test map: not created
R2: not started
```

## Completed R0 Boundary

- Created `docs/UIA2NNativeHUDBaseline.md` with the sealed evidence map and the
  visible/validation/Finish/Cancel/invalid/exact-token contract for every Record.
- UE5.8 UMGToolSet read-only export confirmed exactly 75 HUD Designer Widgets,
  including real type and `IsVariable` values: 33 true, 42 false.
- Native classification is complete: 23 Required BindWidget, 6
  BindWidgetOptional, 46 Designer-only.
- `Txt_DamagePresentation` is a `UMG.TextBlock` and the current disk asset has
  `IsVariable=true`. No Legacy edit is required before the future Native duplicate
  binds it.
- The single injection point remains `ABattleHUDPresenter::WidgetClass`.
- The non-production strategy is locked: R2 will create `L_BattleTest_Native` and
  override only that map's Presenter instance to `WBP_BattleHUD_Native`.
- Production `L_BattleTest`, `BP_BattleHUDPresenter`, and `DefaultEngine.ini`
  remain on `WBP_BattleHUD`. No runtime Legacy/Native toggle or second Controller
  assembly path was added.

## R0 Validation Evidence

Legacy asset hashes after the R0 documentation work and PIE were unchanged:

```text
WBP_BattleHUD
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

The R0 UE5.8 floating PIE smoke used the formal production map and UI interaction:

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

## R1 Source Implementation

The shared base received only the additive backward-compatible extension required by
R1:

```text
UBattleHUDWidgetBase::HandleViewModelChanged
→ preserve bSuppressPresentationCancellation / CancelTrackedPresentationPlayback
→ NativeOnBattleHUDViewModelChanged

UBattleHUDWidgetBase::NativeOnBattleHUDViewModelChanged default
→ BP_OnViewModelChanged
```

Changed runtime files:

```text
Source/SlayTheSpireDemo/UI/BattleHUDWidgetBase.h
Source/SlayTheSpireDemo/UI/BattleHUDWidgetBase.cpp
```

The source diff adds one protected virtual native hook and redirects the existing
post-cancellation refresh call through it. It does not change the existing Blueprint
`BP_OnViewModelChanged` event contract, playback token ownership, cancellation
suppression, Controller calls, ViewModel ownership, or any Record/Envelope type.

No `UBattleHUDWidget`, Native WBP, Native Card/Status Widget, test map, Record handler,
or UI-A3 implementation was created.

## R1 Validation Status

Not yet accepted. The GitHub-only execution surface cannot run the repository's local
UE5.8 project-file generation, Editor build, Blueprint/PIE regression or inspect the
runtime Legacy Widget instance after this source change.

Required R1 validation remains exactly:

```text
1. Regenerate project files with the bundled UE 5.8 .NET runtime.
2. Build SlayTheSpireDemoEditor Win64 Development.
3. Open/compile the existing Legacy WBP_BattleHUD without modifying or saving
   unrelated asset state.
4. PIE /Game/SlayTheSpireDemo/Maps/L_BattleTest using the production
   WidgetClass = WBP_BattleHUD.
5. Verify the Legacy Blueprint still receives Battle HUD View Model Changed and
   refreshes the initial HUD normally.
6. Exercise a normal committed presentation completion and explicit Skip path;
   neither may be interpreted as visual Cancel during the synchronous ViewModel
   update.
7. Exercise or reuse the smallest valid fail-safe path that causes a non-suppressed
   ViewModel change while a tracked visual is active; the abandoned visual must
   still be cancelled before the Legacy Blueprint refresh.
8. Confirm WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus hashes remain unchanged
   if no intentional asset save is required.
```

Do not mark R1 COMPLETE / VALIDATED and do not enter R2 until this evidence exists.

## Next Exact Action — Finish R1 Validation

Run the R1 UE5.8 build/Legacy regression gate above on commit
`496224de8fa549e7ac3563adf04e58743f072b85` (branch `a2n/r1-native-hook`).

If all evidence passes, record it and merge/fast-forward the reviewed R1 source into
the intended integration branch before beginning R2. If validation fails, fix only
the R1 shared-base hook boundary; do not expand into R2.

## Blockers

R1 source implementation itself has no known code blocker. R1 acceptance is pending
because the required UE5.8 Editor/build/PIE environment is outside this GitHub-only
execution surface.
