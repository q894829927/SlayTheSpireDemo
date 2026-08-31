# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-08-31**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

Goal execution status: **IN PROGRESS — R0 COMPLETE / VALIDATED; R1 COMPLETE / VALIDATED; R2 NOT STARTED**.

## Current Repository State

```text
Base branch: main
R0 starting HEAD: 4e977f3af3980d7d534867d737a6b78539c92314
R0 checkpoint HEAD: de30f278b405f2cab6f96fb4e88a84acc53cfd49
R1 working branch: a2n/r1-native-hook
R1 source implementation commit: 496224de8fa549e7ac3563adf04e58743f072b85
R1 source subject: refactor(ui-a2n): add native HUD refresh hook
R1 validation result: PASS
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

## R1 Validation Evidence — PASS

The complete R1 UE5.8 acceptance gate was executed locally and all required checks
passed:

```text
1. Project-file regeneration: PASS
2. SlayTheSpireDemoEditor Win64 Development build: PASS
3. Existing WBP_BattleHUD compile: PASS
4. Initial Legacy HUD PIE on /Game/SlayTheSpireDemo/Maps/L_BattleTest: PASS
5. Strike -> Enemy committed presentation / Legacy refresh path: PASS
6. Normal Finish / explicit Skip / cancellation-suppression behavior: PASS
7. Fail-safe active Cancel with exact abandoned Token: PASS
8. Git status / Legacy HUD-Card-Status hash stability: PASS
```

The fail-safe cancellation evidence reused the existing automation test:

```text
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout
```

This test verifies both sides of the R1 cancellation boundary:

```text
Controller timeout while a tracked visual is active
→ ViewModel advances
→ exact abandoned visual is cancelled once with the exact Token

Normal deferred completion
→ ViewModel advances under suppression
→ no fail-safe visual Cancel is issued
```

The local Legacy PIE checks additionally confirmed that the real production
`WBP_BattleHUD` still refreshes through the existing Blueprint event contract after
routing through `NativeOnBattleHUDViewModelChanged`, and that the production
WidgetClass remains Legacy.

Legacy assets remained unchanged after the R1 validation pass. The sealed R0 hashes
remain the expected values:

```text
WBP_BattleHUD
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

## R1 Acceptance

**R1 is COMPLETE / VALIDATED.**

Accepted properties:

- the Legacy Blueprint refresh contract is preserved;
- non-suppressed ViewModel changes still cancel the exact tracked visual first;
- normal completion / Skip suppression does not create a false Cancel;
- no Legacy WBP asset was reparented or modified;
- production still uses `WBP_BattleHUD`;
- no R2 implementation was started as part of R1.

## Next Exact Action — R2 Native HUD Shell

R2 may now begin under `docs/Phase6UIA2NNativeHUDRefactor.md`.

The next phase must preserve the dual-asset stack:

```text
Legacy WBP_BattleHUD
→ remains frozen on UBattleHUDWidgetBase

WBP_BattleHUD_Native
→ duplicate Legacy asset
→ reparent only the duplicate to UBattleHUDWidget
→ retain Designer hierarchy / Slots / animations / resources / Widget names
→ remove only the duplicate's Legacy runtime Graph ownership
```

Production must remain on the Legacy `WBP_BattleHUD` throughout R2. The Native stack
must use the locked non-production test injection path; do not add a player-visible
Legacy/Native runtime toggle.

## Blockers

No R1 blocker remains. R2 has not started.
