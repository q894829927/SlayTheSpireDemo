# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-09-01**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

Goal execution status: **IN PROGRESS — R0-R8 COMPLETE / VALIDATED; R9 NOT STARTED**.

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
R8 final Editor build: PASS (user confirmed 2026-09-01)
R8 focused Automation: 6/6 PASS (user confirmed 2026-09-01)
R8 Manual PIE: PASS / sticky (user confirmed 2026-09-01)
Production map: /Game/SlayTheSpireDemo/Maps/L_BattleTest
Production WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD_C
Native test map: /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
Native test WidgetClass: /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
R5: COMPLETE / VALIDATED
R6: COMPLETE / VALIDATED
R7: COMPLETE / VALIDATED
R8: COMPLETE / VALIDATED
R9 and later: NOT STARTED
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
final: Energy 4/5, Enemy HP 100/100→94/100, Discard 0→1, Hand 5→4
ViewModel: stateRevision=5 / interactionState=Idle / bInputLocked=false / outcome=None
HUD cleanup: active type/token/timer and Card/Status transient references cleared
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
remain the expected values.

## R1 Acceptance

**R1 is COMPLETE / VALIDATED.**

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
- `CardWidgetClass` and `StatusWidgetClass` typed selectors with no hard-coded WBP object path in C++;
- fail-closed runtime validation using `ensureMsgf` and `UE_LOG(Error)`;
- a Native ViewModel hook that deliberately does not call the Legacy Blueprint refresh;
- an unmigrated playback implementation that returns `false` and starts no async state.

The Card/Status native classes are type-only R2 shells. They contain no R4 Card view,
delegate or input behavior and no R9 frozen Status view, identity or lifecycle rule.

All three Native WBP assets were produced by duplicating the Legacy Designer assets,
reparenting only the duplicates, and removing business graph ownership from the
duplicates. Reloaded UE5.8 asset inspection confirmed the expected Native parents,
Designer counts, and empty EventGraphs.

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

**R2 is COMPLETE / VALIDATED.**

## R3-A Implementation / Validation — PASS

R3-A moved only static HUD refresh and long-lived input ownership into
`UBattleHUDWidget`; no Hand/Card behavior or Presentation Record playback was added.
The Native stack consumes frozen `UBattleHUDViewModel` state, owns the long-lived
EndTurn/Confirm/Cancel/combatant delegates, and does not enter Legacy
`BP_OnViewModelChanged`.

The required Editor Build, Native Blueprint compile, Native PIE static refresh/input
checks and later focused review fixes passed. The review-fix suite:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable: PASS
4/4 PASS
```

**R3-A is COMPLETE / VALIDATED.**

## R4 Implementation / Validation — PASS

R4 moves only formal Hand/Card display and input ownership into the Native stack.
`UBattleCardWidget` owns its supplied `FBattleHUDCardView` plus one UI request event;
`UBattleHUDWidget::RefreshHand` rebuilds formal Hand from frozen ViewModel state and
forwards exact RuntimeId selection through the existing base API.

The user completed the Editor Build, Native Card/HUD Blueprint compile, focused R4
Automation and Native PIE interaction gate. Detailed evidence is recorded in
`docs/R4NativeCardHandValidation.md`.

**R4 is COMPLETE / VALIDATED.**

## R5 Implementation / Validation — PASS

R5 establishes the Native HUD local committed-presentation playback kernel only:
exact local Token/type/timer state plus safe Begin/Finish/Cancel/destruction
boundaries. It does not copy Controller authority.

The final R5 gates passed:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: PASS
SlayTheSpireDemo.Phase6UIA2N.R5 focused Automation: 4/4 PASS
L_BattleTest_Native minimal PIE smoke: PASS
```

Detailed evidence is in `docs/R5NativePlaybackKernelValidation.md`.

**R5 is COMPLETE / VALIDATED.**

## R6 Implementation

R6 migrates only `EnergyChanged`, `BlockChanged` and `DeckShuffled` in the Native
HUD. All handlers validate their frozen payload and required historical Before state
before ownership, reuse the R5 exact-token timer kernel, display frozen After on
Begin/Finish, and restore frozen Before on exact Cancel without normal completion
Notify.

## R6 Automated Validation Evidence — PASS

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: NOT REQUIRED
SlayTheSpireDemo.Phase6UIA2N.R6 focused Automation: 5/5 PASS
```

The user confirmed the required minimal PIE pass on **2026-08-31** in
`/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native`.

**R6 is COMPLETE / VALIDATED.**

## R7 Implementation

R7 migrates only `Damage` in the Native HUD. It resolves exact
`TargetPresentationId`, validates frozen historical HP/Block Before, displays frozen
IncomingDamage/HPAfter/BlockAfter, and never derives committed values from live
Gameplay or from IncomingDamage.

## R7 Automated Validation Evidence — PASS

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native targeted compile: NOT REQUIRED
SlayTheSpireDemo.Phase6UIA2N.R7 focused Automation: 5/5 PASS
```

The user confirmed the required minimal Damage PIE pass on **2026-08-31**.

**R7 is COMPLETE / VALIDATED.**

## R8 Implementation

R8 migrates only `CardPlayed` and `CardZoneChanged` in the Native HUD. It validates
exact frozen card identity/index/count/energy data and uses presentation-only,
HitTestInvisible cards with no HUD request delegate.

Each Draw Record creates exactly one transient at its exact `ToIndex`, moves it from
the Draw visual anchor to Hand, and exact-token Finish releases Controller to apply
only that Record's snapshot before the next Draw begins. Consecutive draws therefore
remain strictly one Record/card at a time.

The first Manual PIE pass exposed missing non-Draw motion. The lifecycle correction
added Hand-to-PlayArea, Hand-to-Discard, PlayArea-to-Discard and Exhaust/Removed
movement/retirement cues. The corrected manual PIE then passed and remains sticky.

A later narrow review found one P1 cleanup gap: `NativePlayedCardWidget` intentionally
survives CardPlayed Finish for the later PlayArea destination, but a Skip/fail-safe
exact Cancel during a later Record could leave that retained card in `OV_PlayArea`.
The centralized exact-Cancel fix now retires any retained PlayedCard after current
Record type-specific Cancel and before local ownership is cleared. Wrong/stale Token
Cancel still returns before this cleanup.

P1 commits:

```text
ec361b0ea67a96b423e0c710399e18080779e1e7
  fix(ui-a2n): clear retained played card on cancel

d1a48d486ea80cf759e6556396df4124805cd06f
  test(ui-a2n): cover R8 skip transient cleanup
```

## R8 Final Automated Validation Evidence — PASS

The runtime P1 fix invalidated only Editor Build and the focused R8 Automation gate.
The user reran both on **2026-09-01** and confirmed:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleHUD_Native / WBP_BattleCard_Native targeted compile: NOT REQUIRED
SlayTheSpireDemo.Phase6UIA2N.R8 focused Automation: 6/6 PASS
0 failed / 0 notRun
```

Focused cases:

```text
CardPlayed.ExactIdentityFinishAndCancel:        PASS
CardPlayed.InvalidIdentityZeroSideEffects:      PASS
Zone.DrawToHandSequentialPresentation:          PASS
Zone.HandToDiscardFinishCancelAndInvalid:       PASS
Zone.PlayAreaDestinationsAndDestruct:           PASS
Zone.SkipClearsRetainedPlayedCard:               PASS
```

The final focused suite covers exact identity, invalid zero-side-effect Begin,
strict serial Draw, noninteractive presentation cards, lifecycle animations,
Finish/Cancel/Destruct cleanup, stale/wrong token behavior, destination retirement,
and the cross-Record Skip cleanup regression.

The corrected minimal R8 PIE remained valid because the P1 fix changes only
abandoned/Skip cleanup and does not alter normal visual paths.

```text
R8 COMPLETE / VALIDATED
R9 NOT STARTED
```

Detailed evidence is in `docs/R8NativeCardLifecycleValidation.md`.

## Next Exact Action — STOP

Wait for explicit user authorization before starting R9. Do not enter R9 or any
later phase automatically.

## Blockers

No R8 blocker remains. R9 and all later phases remain NOT STARTED.
