# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-08-31**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

Goal execution status: **IN PROGRESS — R0 / R1 / R2 COMPLETE AND VALIDATED; R3-A NOT STARTED**.

## Current Repository State

```text
Base branch: main
R0 starting HEAD: 4e977f3af3980d7d534867d737a6b78539c92314
R0 checkpoint HEAD: de30f278b405f2cab6f96fb4e88a84acc53cfd49
R1 working branch: a2n/r1-native-hook
R1 source implementation commit: 496224de8fa549e7ac3563adf04e58743f072b85
R1 source subject: refactor(ui-a2n): add native HUD refresh hook
R1 validation result: PASS
R2 starting HEAD: ad37b0e668a624c827c747f5c8c1166a70c6e109
R2 source implementation commit: d15287ec068f699390a4f64cfab824dcbe53980b
R2 source subject: refactor(ui-a2n): add native HUD shell
R2 validation result: PASS
Production map: /Game/SlayTheSpireDemo/Maps/L_BattleTest
Production WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD_C
Native test map: /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
Native test WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
R3-A: not started
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

## R2 Implementation

R2 created the minimal Native ownership shells required by the dedicated plan:

```text
UBattleHUDWidget
UBattleCardWidget shell only
UBattleStatusWidget shell only

WBP_BattleHUD_Native
WBP_BattleCard_Native
WBP_BattleStatus_Native
L_BattleTest_Native
```

`UBattleHUDWidget` owns only the R2 Designer binding contract and runtime validation:

- 23 required `BindWidget` controls and 6 `BindWidgetOptional` controls;
- `CardWidgetClass` and `StatusWidgetClass` typed selectors with no hard-coded WBP
  object path in C++;
- fail-closed runtime validation using `ensureMsgf` and `UE_LOG(Error)`;
- a Native ViewModel hook that deliberately does not call the Legacy Blueprint
  refresh;
- an unmigrated playback implementation that returns `false` and starts no async
  state.

The Card/Status native classes are type-only R2 shells. They contain no R4 Card view,
delegate or input behavior and no R9 frozen Status view, identity or lifecycle rule.

All three Native WBP assets were produced by duplicating the Legacy Designer assets,
reparenting only the duplicates, and removing business graph ownership from the
duplicates. Reloaded UE5.8 asset inspection confirmed:

```text
WBP_BattleHUD_Native
parent=/Script/SlayTheSpireDemo.BattleHUDWidget
widgetCount=75, graphCount=1, EventGraph nodes=0

WBP_BattleCard_Native
parent=/Script/SlayTheSpireDemo.BattleCardWidget
widgetCount=20, graphCount=1, EventGraph nodes=0

WBP_BattleStatus_Native
parent=/Script/SlayTheSpireDemo.BattleStatusWidget
widgetCount=4, graphCount=1, EventGraph nodes=0
```

The Native HUD defaults resolve to `WBP_BattleCard_Native_C` and
`WBP_BattleStatus_Native_C`. Only the Presenter instance in `L_BattleTest_Native`
overrides `WidgetClass` to `WBP_BattleHUD_Native_C`.

## R2 Validation Evidence — PASS

The complete R2 gate was executed on the saved final implementation:

```text
1. UE 5.8 bundled project-file regeneration: PASS
2. SlayTheSpireDemoEditor Win64 Development build: PASS
3. Native HUD/Card/Status Blueprint compile and save: PASS
4. Reloaded parent / graph / Designer-count inspection: PASS
5. Native L_BattleTest_Native floating PIE: PASS
6. Existing Presenter created Native Widget + ViewModel + Controller: PASS
7. Required binding fail-closed log check: PASS, zero errors
8. Focused SlayTheSpireDemo.Phase6UIA2A: PASS
9. Production Legacy configuration and hashes: PASS
10. Independent architecture review: PASS, no P0/P1 blocker
```

Native PIE runtime inspection returned:

```text
WidgetClass = WBP_BattleHUD_Native_C
WidgetInstance = WBP_BattleHUD_Native_C_0
ViewModel = BattleHUDViewModel_0
PresentationController = BattlePresentationController_0
```

The final PIE log records `No blueprints needed recompiling`, creation of
`UEDPIE_0_L_BattleTest_Native`, successful server login, and no
`[BattleHUD][Native]`, ensure, BindWidget, Blueprint or UMG error.

Focused regression evidence:

```text
SlayTheSpireDemo.Phase6UIA2A
8 total
3 succeeded
5 succeededWithWarnings
0 failed
0 notRun

Saved/AutomationReports/R2FocusedPhase6UIA2A/index.json
Saved/Logs/R2FocusedPhase6UIA2A.log
```

Legacy asset hashes after R2 remain:

```text
WBP_BattleHUD
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

After PIE the Editor returned to formal `L_BattleTest`; both its Presenter instance
and `BP_BattleHUDPresenter` default still use `WBP_BattleHUD_C`. `DefaultEngine.ini`
still uses `L_BattleTest` for Editor and Game maps. No production or Legacy asset was
modified.

The architecture review recorded one non-blocking migration residue: the duplicated
assets still retain unexecuted Legacy member variables. Their business graphs are
empty, so they own no runtime behavior in R2. Each applicable later ownership phase
must take over or remove its residue; do not turn this into an unscheduled R2 cleanup.

## R2 Acceptance

**R2 is COMPLETE / VALIDATED.**

## Next Exact Action — R3-A Static HUD and Long-Lived Delegates

Implement only the R3-A scope in `UBattleHUDWidget`:

```text
RefreshHUDFromViewModel
RefreshCombatants
RefreshEnergy
RefreshPileCounts
RefreshInputState
RefreshFeedback
RefreshTerminalFromViewModel

EndTurn / Confirm / Cancel
Combatant target / inspect long-lived delegates
```

Bind long-lived delegates once in `NativeConstruct`, unbind in `NativeDestruct`, and
continue through the formal `UBattleHUDWidgetBase` request APIs. Production remains
Legacy and R3-A uses only `L_BattleTest_Native`.

Do not implement `RefreshHand`, Native Card input, the playback kernel, any
Presentation Record, Card/Status lifecycle, terminal Record sequencing, UI-A3, or
production cutover in R3-A.

## Blockers

No R2 blocker remains. R3-A has not started.
