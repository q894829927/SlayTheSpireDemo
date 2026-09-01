# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-09-01**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

Goal execution status: **R0-R13 COMPLETE / VALIDATED; Native HUD is the production default; Legacy assets retained; R14-A IN PROGRESS; R14-A1 COMPLETE / VALIDATED; R14-A2 COMPLETE / VALIDATED; R14-B NOT AUTHORIZED; UI-A3 NOT STARTED**.

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
R3-A starting HEAD: e0ac820245e8ea93128507f058316e32c5aaf427
R3-A validation result: PASS
R3-A review-fix branch: a2n/r3-review-fix
R3-A review-fix focused result: 4/4 PASS
R4 working branch: a2n/r4-native-card-hand
R4 base main: 9981dcebda27ae5be46be608177084412e78b1fb
R4 validation result: PASS
R5 working branch: a2n/r5-native-playback-kernel
R5 base main: 1978e1d3abe831dedef95b8bd431a7717def573b
R5 timer-binding fix: 21e3f7dca0d72c8687465fce10892e205774f893
R5 validation result: PASS
R6 starting HEAD: 778073be41ffa0c003cdab5fde9ca1d1ac996cb8
R6 source implementation commit: 1250cb411afe640802d7b70239a51228a94ed369
R6 Editor build: PASS
R6 focused Automation: 5/5 PASS
R6 Manual PIE: PASS (user confirmed 2026-08-31)
R7 working branch: a2n/r7-native-damage
R7 starting main HEAD: 2264b9e5ba8b6505fffcef5abed21d2d6bdc7611
R7 source implementation commit: c3a345413a87197de8328eb94e6b849d365f5442
R7 Editor build: PASS
R7 focused Automation: 5/5 PASS
R7 Manual PIE: PASS (user confirmed 2026-08-31)
R8 working branch: a2n/r8-native-card-lifecycle
R8 starting main HEAD: 22f0955787551b0c5a3201f9ca45cf35e5167cbf
R8 source implementation commit: c1d621b01e5d5cfc8b680181e9f191edb300373c
R8 lifecycle-animation correction commit: c929e6b3961b36b004bfcd224fe4a02421577e80
R8 P1 cross-Record cleanup fix: ec361b0ea67a96b423e0c710399e18080779e1e7
R8 P1 cleanup regression test: d1a48d486ea80cf759e6556396df4124805cd06f
R8 Editor build: PASS (final P1-revalidated head; user confirmed 2026-09-01)
R8 focused Automation: 6/6 PASS (user confirmed 2026-09-01)
R8 Manual PIE: PASS / sticky (user confirmed 2026-09-01)
R9 implementation branch: main (explicitly authorized; no working branch)
R9 starting main HEAD: b05a7d1281e921eed2bbb4bf5238842fa16421f7
R9 source implementation head: ea209d1aea58210b26057017ba13aa6e8e84385a
R9 Editor build: PASS (user confirmed 2026-09-01)
R9 WBP_BattleStatus_Native compile: PASS (user confirmed 2026-09-01)
R9 focused Automation: 5/5 PASS (user confirmed 2026-09-01)
R9 Manual PIE: PASS (user confirmed 2026-09-01)
R10 validation result: COMPLETE / VALIDATED (see R10 validation document)
R11 starting HEAD: 08d6fc003701e485ef37414d2ac79ba8a436d3cb
R11 Scenario A-E Legacy/Native parity: PASS (user confirmed 2026-09-01)
R11 temporal Skip deterministic PIE: PASS (Legacy + Native, 2026-09-01)
R11 temporal Cancel/stale deterministic PIE: PASS (Legacy + Native, 2026-09-01)
R11 temporal visual observation: PASS (Legacy + Native; user confirmed 2026-09-01)
R11 A2D5 aggregate: 6/6 PASS (user confirmed 2026-09-01)
R11 Native aggregate: 35/35 PASS (user confirmed 2026-09-01)
R11 Native WBP compile/save: 3/3 PASS (user confirmed 2026-09-01)
R11 temporary harness cleanup Editor build: PASS (Codex, 2026-09-01)
R12-A cutover commit: de788c5b68e06827f8fdba3b83858f86a385bdeb
R12-A production map-only cutover: PASS
R12-B Editor build: PASS
R12-B Native WBP compile/save/reopen: 3/3 PASS
R12-B Phase6UIA2D5: exactly 6/6 PASS
R12-B formal Phase6R: exactly 100/100 PASS
R12-B production-map manual PIE: PASS (user confirmed 2026-09-01)
R12-B post-harness-cleanup Editor build: PASS
R12-B post-harness-cleanup clean Shipping exclusion: PASS
R13 starting HEAD: 76d411a21c042a86d1e7a4c608a67ae10c724ea2
R13 milestone: R13-M1 — Native Production Stabilization
R13 post-cutover Native-only UI change: fe7fe4e (production Native dependency stabilization)
R13 Legacy runtime fallback occurred: NO
R13 production Legacy HUD/Card/Status dependencies: 0 — PASS
R13 Editor build after Native-only change: PASS
R13 objective tests: exactly 1/1 PASS, 0 failed, 0 notRun
R13 formal Phase6R: exactly 100/100 test state Success, 0 failed, 0 notRun — PASS
R13 pre-manual-harness clean Shipping: PASS; forbidden artifact hits 0
R13 final production-map Scenario A-E: PASS (user confirmed 2026-09-01)
R13 active Skip: PASS
R13 active timeout Cancel + stale callback rejection: PASS
R13 Input Unlock after catch-up: PASS
R13 temporary Editor-only PIE commands: REMOVED / NEVER COMMITTED
R13 final post-harness-cleanup Editor build: PASS
R13 final post-harness-cleanup clean Shipping exclusion: PASS; forbidden artifact hits 0
Production map: /Game/SlayTheSpireDemo/Maps/L_BattleTest
Production WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
Native test map: /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
Native test WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
R5: COMPLETE / VALIDATED
R6: COMPLETE / VALIDATED
R7: COMPLETE / VALIDATED
R8: COMPLETE / VALIDATED
R9: COMPLETE / VALIDATED
R10: COMPLETE / VALIDATED
R11: COMPLETE / VALIDATED
R12-A: COMPLETE
R12-B: COMPLETE / VALIDATED
R13: COMPLETE / VALIDATED
R14-A starting HEAD: e1b60480807ae1a140acc637a5873990d2937722
R14-A1: COMPLETE / VALIDATED
R14-A2 starting HEAD: 2ee470e
R14-A2 Native WBP compile/save/reopen: 3/3 PASS, BS_UP_TO_DATE, 0 errors
R14-A2 Editor Build: PASS
R14-A2 R4 focused Automation: exactly 1/1 PASS, 0 failed, 0 notRun
R14-A2 R9 focused Automation: exactly 5/5 PASS, 0 failed, 0 notRun
R14-A2 R13 AssetReferences: exactly 1/1 PASS, 0 failed, 0 notRun
R14-A2 production Legacy HUD/Card/Status dependency count: 0
R14-A2 production PIE smoke: PASS (user confirmed 2026-09-01)
R14-A2 modification commit: 8a609659ba138c922fe64bbfd08bca44b05ca8d6
R14-A2: COMPLETE / VALIDATED
R14-A: IN PROGRESS
R14-B: NOT AUTHORIZED
UI-A3: NOT STARTED
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

- 23 required `BindWidget` controls and 6
  `BindWidgetOptional` controls;
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

## R3-A Implementation

R3-A moved only static HUD refresh and long-lived input ownership into
`UBattleHUDWidget`; no Hand/Card behavior or Presentation Record playback was
added. The implementation consumes only the frozen `UBattleHUDViewModel` fields:

```text
RefreshHUDFromViewModel
→ RefreshCombatants / RefreshEnergy / RefreshPileCounts
→ RefreshInputState / RefreshFeedback / RefreshEnemyIntent
→ RefreshTerminalFromViewModel
```

`NativeOnBattleHUDViewModelChanged()` calls the Native refresh directly and does
not call `Super`, so the Native stack cannot enter Legacy `BP_OnViewModelChanged`.
Required bindings remain fail-closed. EndTurn, Confirm, Cancel, combatant target,
inspect and inspect-cleared delegates use `AddUniqueDynamic` once in
`NativeConstruct`, matching `RemoveDynamic` calls in `NativeDestruct`; ViewModel
refresh never binds delegates. All requests continue through the existing
`UBattleHUDWidgetBase` APIs.

Changed source files:

```text
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDWidget.cpp
```

R3-A intentionally did not implement `RefreshHand`, Native Card input,
`UBattleCardWidget` behavior, Playback Kernel, Presentation Records, Status
lifecycle, Terminal Record sequencing, Controller/Reducer/Record changes,
production cutover or UI-A3.

## R3-A Validation Evidence — PASS

The required R3-A checks were run against the Native test stack only:

```text
1. UE 5.8 SlayTheSpireDemoEditor Development build: PASS
   Build.bat ... SlayTheSpireDemoEditor Win64 Development
   Result: Succeeded
2. CompileAllBlueprints: PASS
   WBP_BattleHUD_Native compiled successfully;
   final commandlet summary: 0 errors, 0 warnings, 0 failed blueprints.
3. Native PIE map /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native: PASS
   UEDPIE_0_L_BattleTest_Native was created and the Native HUD was visible.
4. Frozen static refresh: PASS
   initial Player 80/80, Enemy 100/100, Energy 5/5;
   TestAttack refreshed Enemy 94/100 and Energy 4/5;
   after EndTurn and enemy resolution Player 74/80 and Energy 5/5;
   pile count surfaces remained consistent with the ViewModel.
5. Input handlers: PASS
   EndTurn produced the real Player turn-ending commit and next ReadStateReady;
   Native target handler produced the frozen invalid-target feedback;
   Confirm and Cancel handlers were each invoked once on the Native instance.
6. Combatant inspection: PASS
   hovering the enemy presentation surfaced its frozen display name and clearing
   the inspection restored the optional surface.
7. Native-only ownership / delegate audit: PASS
   no Super call from NativeOnBattleHUDViewModelChanged; no refresh-time binding;
   one AddUniqueDynamic binding boundary and one NativeDestruct removal boundary.
8. Teardown: PASS
   PIE stopped cleanly after the Native run.
```

Runtime evidence is retained in `Saved/Logs/SlayTheSpireDemo.log` (Native PIE
start, `TestAttack`, EndTurn commit, handler calls and teardown) and the local
inspection captures under `Saved/Screenshots/WindowsEditor/`.

The production boundary remained untouched:

```text
Config/DefaultEngine.ini: EditorStartupMap and GameDefaultMap = L_BattleTest
production WidgetClass = WBP_BattleHUD_C
Legacy WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus: no diff
```

Legacy asset hashes after the R3-A run remain exactly the sealed values:

```text
WBP_BattleHUD
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

A2D5, Phase6R and Shipping were not rerun; their sealed evidence remains the
R0/R1/R2 baseline evidence.

## R3-A Review Fix Validation Evidence — PASS

The review fix preserves the R3 boundary while closing the two missing parity items:
frozen combatant Status tooltip rebuilding and complete zero-Block badge visibility.
Permanent Editor-only tests were added under the `SlayTheSpireDemoTests` module.

The user rebuilt the saved review-fix branch on UE 5.8 and then ran:

```text
SlayTheSpireDemo.Phase6UIA2N.R3
```

with the following result:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable: PASS

4/4 PASS
```

This closes the formal R3-A review gaps:

- Block `0` collapses the full shield badge; positive Block restores the exact value;
- inspect tooltip consumes the frozen `CombatantView.Statuses` DTO and clears on inspect end;
- terminal None/Victory/Defeat/ResolutionFaulted surfaces render from ViewModel state;
- PresentationUnavailable locks input and renders feedback while remaining distinct from ResolutionFaulted.

The review-fix diff does not add Hand/Card ownership, Damage playback, formal Status-row lifecycle, Controller/Reducer/Record changes, production cutover, or UI-A3 work.

**R3-A is COMPLETE / VALIDATED.**

## R4 Implementation

R4 moves only formal Hand/Card display and input ownership into the Native stack.

`UBattleCardWidget` now owns only its supplied `FBattleHUDCardView` and the UI request event:

```text
SetCardView(FBattleHUDCardView)
GetRuntimeId()
GetCardId()
GetCardView()
OnBattleCardRequested(RuntimeId)
```

`UBattleHUDWidget::RefreshHand` now rebuilds `HB_Hand` exclusively from `ViewModel.HandCards`, creates `CardWidgetClass` instances, assigns the DTO, binds each formal Card request exactly once, and forwards only the exact RuntimeId through `UBattleHUDWidgetBase::SelectCard`.

The Native Card has no HUD, ViewModel, Gameplay card instance, BattleManager, PresentationController, Record or Envelope reference. The Legacy `OwnerHUD : WBP_BattleHUD` dependency is not reintroduced.

R4 intentionally does not create presentation-only cards and does not migrate any committed Record playback. `BeginPresentationRecordPlayback_Implementation` remains the false/immediate-fallback boundary.

## R4 Validation Evidence — PASS

The user completed the requested UE5.8 R4 acceptance gate on the saved R4 branch:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleCard_Native compile: PASS
3. WBP_BattleHUD_Native compile: PASS
4. SlayTheSpireDemo.Phase6UIA2N.R4 focused Automation: PASS
5. L_BattleTest_Native initial Hand rebuild/display parity: PASS
6. Card Name / Cost / Type / Description / Art surfaces: PASS
7. exact RuntimeId formal SelectCard request path: PASS
8. ChoosingTarget / legal-target highlight / Cancel behavior: PASS
9. accepted legal-target submission occurs once: PASS
10. no duplicate dynamic Card callback after refresh/rebuild: PASS
11. R3 zero-Block badge behavior remains correct: PASS
12. production L_BattleTest remains on WBP_BattleHUD_C: PASS
13. Legacy WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus remain unchanged: PASS
14. BeginPresentationRecordPlayback remains false / unmigrated: PASS
15. R5 and later remain NOT STARTED: PASS
```

Focused Editor-only coverage includes:

```text
SlayTheSpireDemo.Phase6UIA2N.R4.CardWidget.DTOAndRequest
```

Detailed R4 implementation and acceptance evidence is recorded in:

```text
docs/R4NativeCardHandValidation.md
```

**R4 is COMPLETE / VALIDATED.**

## R5 Implementation

R5 establishes the Native HUD's local committed-presentation playback kernel only.
It owns exact local Token/type/timer state and the safe Begin/Finish/Cancel/destruction
boundaries that later Record-specific phases will reuse. Production Native playback
still accepts no real Record in R5, so every real Record continues through the
Controller's existing immediate-fallback path.

The kernel does not copy Controller queue/reducer/WorkingSnapshot/generation/timeout
authority into the HUD and does not change Gameplay, Record or Envelope semantics.

The first local build exposed a UE5.8 timer-delegate payload signature mismatch. The
fix changed only the finish timer binding to a weak lambda that captures the exact
`FPresentationPlaybackToken` by value and forwards it to the existing exact-token
finish handler.

## R5 Validation Evidence — PASS

The user completed the corrected R5 acceptance gates:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleHUD_Native targeted compile: PASS
3. SlayTheSpireDemo.Phase6UIA2N.R5 focused Automation: PASS
4. L_BattleTest_Native minimal PIE smoke: PASS
```

The focused suite covers unsupported/failed Begin zero-side-effect behavior,
exact/wrong-token Cancel, Cancel-without-normal-Notify, duplicate/stale Finish,
old/new Token isolation, timer ownership, and NativeDestruct cleanup.

The minimal PIE smoke confirmed the Native HUD/Hand appear normally, card selection
can be cancelled back to an operable state, EndTurn remains usable, and there is no
crash, permanent input lock, duplicate Hand, blank HUD, or broken immediate fallback.

Detailed R5 implementation and acceptance evidence is recorded in:

```text
docs/R5NativePlaybackKernelValidation.md
```

R5 does not claim any R6+ Record visual migration: Energy/Block/Shuffle, Damage,
Card lifecycle, Status lifecycle and terminal Record visuals remain NOT STARTED.

**R5 is COMPLETE / VALIDATED.**

## R6 Implementation

R6 migrates only `EnergyChanged`, `BlockChanged` and `DeckShuffled` in the Native
HUD. All three handlers validate their frozen payload and required historical
Before state before ownership, reuse the R5 exact-token timer kernel, display frozen
After on Begin/Finish, and restore frozen Before on exact Cancel without normal
completion Notify.

`BlockChanged` resolves the Record's exact `TargetPresentationId` for both Player
and Enemy and preserves the complete zero-Block badge collapse rule. `DeckShuffled`
uses only its frozen Draw/Discard transition and does not inspect live Deck zones.
Invalid payload, target or Record/Token metadata returns false with zero local visual
side effects, preserving Controller immediate fallback.

Changed implementation/test files:

```text
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDWidget.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR6TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR6TestTypes.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR6Tests.cpp
```

R6 does not modify Controller, Reducer, Record, Envelope, Gameplay, production map
selection, Legacy WBP assets or UI-A3. Damage and every R7+ visual remain unstarted.

## R6 Automated Validation Evidence — PASS

```text
1. UE5.8 project-file generation: PASS
2. SlayTheSpireDemoEditor Win64 Development build: PASS
3. WBP_BattleHUD_Native targeted compile: NOT REQUIRED
   (no runtime reflected binding/API contract changed)
4. SlayTheSpireDemo.Phase6UIA2N.R6 focused Automation: 5/5 PASS
```

Focused results:

```text
Block:           PASS
DestructCleanup: PASS
Energy:          PASS
InvalidBegin:    PASS
Shuffle:         PASS
0 failed / 0 notRun
```

Evidence is recorded in:

```text
docs/R6NativeEnergyBlockShuffleValidation.md
Saved/AutomationReports/R6FocusedPhase6UIA2N/index.json
```

No R3/R4/R5, A2D5, Phase6R, Shipping or aggregate regression suite was run.

## R6 Manual PIE Validation Evidence — PASS

The user confirmed the required minimal PIE pass on **2026-08-31** in
`/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native`, closing the Energy final value,
Block display, real Shuffle Draw/Discard final counts, and no-flashback / no-
duplicate / no-permanent-Input-Lock / normal-HUD observations.

**R6 is COMPLETE / VALIDATED.**

## R7 Implementation

R7 migrates only `Damage` in the Native HUD. It resolves the Record's exact
`TargetPresentationId` to Player or Enemy, validates frozen historical HP/Block
Before state and the sealed Damage payload invariants, then displays the Record's
`IncomingDamage`, `HPAfter` and `BlockAfter` directly. It does not derive HP/Block
outcomes from IncomingDamage or query mutable Gameplay.

The target receives the sealed Legacy-equivalent `RenderOpacity = 0.45` feedback.
Exact Finish retains frozen After vitals, hides the Damage text, restores opacity,
clears Damage local state and notifies once. Exact Cancel restores frozen Before,
hides the text, restores opacity, clears local state and never notifies. Stale/
duplicate Finish and wrong-token Cancel are no-ops; destruction performs local
transient cleanup only without historical restore or Notify.

Changed implementation/test files:

```text
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDWidget.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR7TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR7TestTypes.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR7Tests.cpp
```

R7 does not modify Controller, Reducer, Record, Envelope, Gameplay, production
configuration, Legacy WBP assets or UI-A3. Card lifecycle and all R8+ visuals remain
unstarted.

## R7 Automated Validation Evidence — PASS

```text
1. UE5.8 project-file generation: PASS
2. SlayTheSpireDemoEditor Win64 Development build: PASS
3. WBP_BattleHUD_Native targeted compile: NOT REQUIRED
   (no runtime reflected binding/API contract changed)
4. SlayTheSpireDemo.Phase6UIA2N.R7 focused Automation: 5/5 PASS
```

Focused results:

```text
DestructCleanup:        PASS
EnemyTarget:            PASS
InvalidBegin:           PASS
Lethal:                 PASS
PlayerBlockedAndCancel: PASS
0 failed / 0 notRun
```

Evidence is recorded in:

```text
docs/R7NativeDamageValidation.md
Saved/AutomationReports/R7FocusedPhase6UIA2N/index.json
```

No R3-R6, A2D5, Phase6R, Shipping, aggregate regression or architecture reviewer was
run.

## R7 Manual PIE Validation Evidence — PASS

The user confirmed the required minimal PIE pass on **2026-08-31** in
`/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native`, closing the single-display Damage
number, correct target/location, transient feedback cleanup, final HP/Block,
no-flashback, no-duplicate-Damage and no-permanent-Input-Lock observations.

**R7 is COMPLETE / VALIDATED.**

## R8 Implementation

R8 migrates only `CardPlayed` and `CardZoneChanged` in the Native HUD. It validates
the exact frozen card snapshot, RuntimeId/CardId/index/count/energy contract and
uses presentation-only, HitTestInvisible cards with no HUD request delegate.
CardPlayed owns the Hand-to-PlayArea transient boundary; the five supported current
producer zone pairs own Hand discard, Draw to Hand, and PlayedCard retirement.

The user-added Draw acceptance contract is implemented without Controller changes:
each Draw Record creates exactly one transient at its exact `ToIndex`, moves it from
the Draw count visual anchor to the final Hand slot, and exact-token Finish releases
Controller to apply only that Record's snapshot before the next Draw begins. Later
drawn cards cannot appear through an early all-at-once Hand refresh.

The first Manual PIE pass found that the non-Draw lifecycle paths still changed
visibility without actual motion. The corrective source now moves CardPlayed from
its exact Hand anchor into centered PlayArea, moves Hand discard and PlayedCard
discard toward `Txt_DiscardCount`, and scales/fades Exhaust/Removed at PlayArea.
All moving cards remain frozen, presentation-only and noninteractive.

A later narrow review found one P1 cross-Record cleanup gap. After exact CardPlayed
Finish, `NativePlayedCardWidget` intentionally survives for the later PlayArea
destination. If a later Record was abandoned through Skip/fail-safe exact Cancel,
that retained PlayedCard could survive Controller collapse. The exact native Cancel
boundary now retires any retained PlayedCard after current Record type-specific
Cancel and before local ownership is cleared; wrong/stale Token Cancel returns before
this cleanup.

P1 fix/test commits:

```text
ec361b0ea67a96b423e0c710399e18080779e1e7
  fix(ui-a2n): clear retained played card on cancel

d1a48d486ea80cf759e6556396df4124805cd06f
  test(ui-a2n): cover R8 skip transient cleanup
```

Changed source/test/design files:

```text
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDWidget.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR8TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR8TestTypes.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR8Tests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR8SkipCleanupTests.cpp
docs/Phase6UIA2NNativeHUDRefactor.md
docs/R8NativeCardLifecycleValidation.md
```

R8 does not modify Gameplay, Controller, reducer, Record/Envelope, Legacy WBP,
production WidgetClass, Status/terminal visuals, UI-A3, or R9+ behavior.

## R8 Final Automated Validation Evidence — PASS

The P1 runtime fix invalidated only Editor Build and the focused R8 Automation gate.
The user reran both on **2026-09-01** and confirmed:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleHUD_Native / WBP_BattleCard_Native targeted compile: NOT REQUIRED
   (no production reflected binding/API contract changed)
3. SlayTheSpireDemo.Phase6UIA2N.R8 focused Automation: 6/6 PASS
   0 failed / 0 notRun
```

Focused results:

```text
CardPlayed.ExactIdentityFinishAndCancel:        PASS
CardPlayed.InvalidIdentityZeroSideEffects:      PASS
Zone.DrawToHandSequentialPresentation:          PASS
Zone.HandToDiscardFinishCancelAndInvalid:       PASS
Zone.PlayAreaDestinationsAndDestruct:           PASS
Zone.SkipClearsRetainedPlayedCard:              PASS
```

The added Skip regression proves:

```text
CardPlayed Finish -> retained PlayedCard
-> later Draw Begin
-> SkipPresentation
-> active Draw transient removed
-> retained PlayedCard removed
-> timer/ownership cleared
-> PlayArea empty
```

No R3-R7, A2D5, Phase6R, Shipping, aggregate regression, reviewer, or R9+ suite was
rerun.

## R8 Manual PIE Validation — PASS / STICKY

The first user pass on **2026-09-01** exposed missing non-Draw movement and did not
close the Gate. After the corrective implementation and affected-Gate rerun, the
user completed the corrected pass in
`/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native`. Hand-to-PlayArea-to-Discard,
Exhaust disappearance, end-turn/manual Hand discard and strict one-card-at-a-time
DrawPile-to-Hand presentation were accepted, with correct final Hand/HUD state and
no flashback, duplicate, transient leak, abnormal HUD, or permanent Input Lock.

The P1 cleanup fix changed only abandoned/Skip cleanup, so this normal visual Gate
remained sticky and did not require another PIE run.

```text
R8 COMPLETE / VALIDATED
R9 COMPLETE / VALIDATED
R10 NOT STARTED
```

## R9 Implementation

R9 migrates only formal Native Status-row ownership and committed `StatusChanged`
presentation. `UBattleStatusWidget` stores only the frozen `FBattleHUDStatusView`
and renders Designer-backed amount/icon state. Native-only member names avoid
colliding with retained Legacy duplicate variables without editing Legacy assets.

The HUD rebuilds formal Player and Enemy Status rows from historical ViewModel data
and resolves lifecycle identity only as:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

Create requires that exact identity to be absent. Increase, reduction and removal
require one matching historical ViewModel Status and one exact formal Widget with a
matching frozen Before view. Update/reduction reuse that Widget; removal collapses
only that exact Widget. Invalid target, identity, reason/flags or historical Before
mismatch returns false with zero local visual side effects.

Exact Finish keeps committed After and notifies through the sealed R5 exact-token
kernel. Wrong/stale Cancel is a no-op. Exact Cancel never reverse-calculates
`B -> A`; it rebuilds both Player and Enemy formal Status rows from the historical
ViewModel and never performs normal completion Notify. Destruction performs only
local transient/reference cleanup.

Changed source/test files:

```text
Source/SlayTheSpireDemo/UI/BattleHUDWidget.h
Source/SlayTheSpireDemo/UI/BattleHUDWidget.cpp
Source/SlayTheSpireDemo/UI/BattleStatusWidget.h
Source/SlayTheSpireDemo/UI/BattleStatusWidget.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR9TestTypes.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR9TestTypes.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR9Tests.cpp
```

R9 does not modify Gameplay, Controller, reducer, Record/Envelope, Legacy WBP,
production WidgetClass, terminal behavior, UI-A3, or R10+ behavior.

## R9 Automated Validation Evidence — PASS

The user ran the required closed-scope R9 gates on **2026-09-01** and confirmed:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleStatus_Native targeted compile: PASS
3. SlayTheSpireDemo.Phase6UIA2N.R9 focused Automation: 5/5 PASS
   0 failed / 0 notRun
```

Focused coverage:

```text
StatusWidget.DTOAndIdentity:                  PASS
Lifecycle.CreateIncreaseReuse:              PASS
Lifecycle.ReductionRemovalAndCancel:        PASS
Identity.NewSequenceAndInvalidFallback:     PASS
Token.StaleAndDestructCleanup:              PASS
```

This covers frozen DTO/identity, create, increase, exact Widget reuse, `2 -> 1`
reduction, `1 -> 0` removal, same StatusId with a later RuntimeSequence, invalid
identity/target/flags/reason zero-side-effect fallback, Player+Enemy historical
Cancel rebuild, wrong-token Cancel, stale Finish and destruction-local cleanup.

No R3-R8, A2D5, Phase6R, Shipping, aggregate regression, reviewer or R10+ suite was
run.

## R9 Manual PIE Validation — PASS

The user confirmed the required minimal visual pass on **2026-09-01** in
`/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native` using existing real Status
producers. Status creation displayed one correct row/icon/amount; same-identity
update/reduction reused one row; reduction such as `2 -> 1` displayed correctly;
`1 -> 0` removed the exact Status. Row/icon/tooltip presentation remained coherent
with no `A -> B -> A` flashback, duplicate Status, abnormal HUD or permanent Input
Lock.

```text
R9 COMPLETE / VALIDATED
R10 NOT STARTED
```

Detailed R9 evidence is recorded in:

```text
docs/R9NativeStatusLifecycleValidation.md
```

## R11 Completion

R11 Scenario A-E manual Legacy/Native parity is PASS. The temporary Editor-only PIE
harness provided stable `A2N.R11.TestSkip` and `A2N.R11.TestCancelStale` entry
points during validation. Isolated in-process PIE runs proved real active playback,
exact timeout Cancel, stale callback isolation, FinalSnapshot and queue catch-up,
and post-catch-up Widget input acceptance for both formal maps.

The user completed both temporal commands on Legacy and Native PIE and confirmed no
visible flashback, abandoned-visual return, duplicate Hand/Status or later-playback
disturbance. The complete temporal parity scope is PASS. The user also confirmed
A2D5 exactly 6/6, the Native R3-R10 aggregate exactly 35/35, and all three Native WBP
compile/save Gates PASS.

The temporary R11 PIE command source and its sole test-module `UnrealEd` dependency
were removed at closure. The affected Editor build was rerun and passed. R11 is
COMPLETE / VALIDATED.

## R12 Completion

R12-A changed only the unique `L_BattleTest` Presenter `WidgetClass` from Legacy to
`WBP_BattleHUD_Native_C`. The isolated production cutover commit is
`de788c5b68e06827f8fdba3b83858f86a385bdeb`.

R12-B ran fresh cutover configuration evidence: Editor build PASS; all three Native
WBP compile/save/reopen PASS; A2D5 exactly 6/6 PASS; the formal twelve-prefix
Phase6R workflow exactly 100/100 PASS; and clean-worktree Shipping exclusion PASS.
The user completed all production-map Scenario A-E, Victory/Defeat, active Skip,
active Cancel, stale callback and Input Unlock manual Gates on 2026-09-01.

The temporary Editor-only R12 PIE harness was never committed. It was deleted after
manual acceptance, after which the affected Editor build and clean-worktree Shipping
Gate passed again on no-harness HEAD
`2fc9f7703bb8bb45e2f75b8f740e646137af0d57`. Native HUD is now the production
default; Legacy HUD/Card/Status assets remain retained.

Detailed evidence: `docs/R12NativeProductionCutoverValidation.md`.

## R13-M1 — Native Production Stabilization

R13-M1 — Native Production Stabilization opened at
`76d411a21c042a86d1e7a4c608a67ae10c724ea2`. The production map instance remains
on `WBP_BattleHUD_Native_C`, all Legacy assets remain retained, and no Legacy runtime
fallback occurred.

The opening post-cutover Git audit found no qualifying change. R13 then implemented
the authorized Native-only production dependency stabilization in commit `fe7fe4e`:
the Presenter default now selects the Native HUD and the Native HUD transient Card/
Status variables use Native concrete types. No Legacy asset was changed.

The opening UE Asset Registry audit found three exact hard-package paths:

```text
L_BattleTest -> BP_BattleHUDPresenter -> WBP_BattleHUD
L_BattleTest -> WBP_BattleHUD_Native -> WBP_BattleCard
L_BattleTest -> WBP_BattleHUD_Native -> WBP_BattleStatus
```

After `fe7fe4e`, loaded-property and Asset Registry inspection reported production
runtime Legacy HUD/Card/Status dependency count `0`. The focused R13 test passed
exactly 1/1, formal Phase6R passed exactly 100/100, and the final production-map
Scenario A-E, active Skip, active timeout Cancel, stale callback and Input Unlock
Gates passed.

The temporary Editor-only PIE commands were removed and never committed. The final
no-harness Editor and Shipping builds passed with zero forbidden Shipping artifacts.
The Skip sequence's HP `80 -> 74 -> 68` is two distinct real EndTurns: the second is
the deliberate post-catch-up input-unlock probe, not duplicate Damage playback.

Detailed milestone state: `docs/R13NativeHUDStabilization.md`.

## R14-A2 Current Slice

The thirteen zero-reference Native Blueprint migration variables were removed only
from `WBP_BattleCard_Native`, `WBP_BattleStatus_Native`, and
`WBP_BattleHUD_Native`. All three Native assets passed compile/save/fresh-reopen
compile. Editor Build, R4, R9, and R13 AssetReferences passed; the production Legacy
HUD/Card/Status dependency count remains zero. Legacy asset hashes remain unchanged.

## Next Exact Action — STOP

R14-A2 is complete. The remaining R14-A cleanup inventory decision is whether
`L_BattleTest_Native` should be retained. Do not delete or modify it without a
separate explicit decision. Do not start R14-B or resume UI-A3.

## Blockers

None for R14-A2.
