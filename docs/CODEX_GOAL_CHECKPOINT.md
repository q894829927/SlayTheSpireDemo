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
R8 Editor build: PASS (final P1-revalidated head; user confirmed 2026-09-01)
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
- Native classification is complete: 23 Required BindWidget, 6 BindWidgetOptional,
  46 Designer-only.
- `Txt_DamagePresentation` is a `UMG.TextBlock` and the current disk asset has
  `IsVariable=true`.
- The single injection point remains `ABattleHUDPresenter::WidgetClass`.
- Production `L_BattleTest`, `BP_BattleHUDPresenter`, and `DefaultEngine.ini`
  remain on `WBP_BattleHUD`.

## R0-R7 Sealed History

R0-R7 remain COMPLETE / VALIDATED with their previously recorded evidence. Their
dedicated validation documents remain authoritative for phase-specific details:

```text
R4: docs/R4NativeCardHandValidation.md
R5: docs/R5NativePlaybackKernelValidation.md
R6: docs/R6NativeEnergyBlockShuffleValidation.md
R7: docs/R7NativeDamageValidation.md
```

The sealed R0 Legacy asset hashes remain:

```text
WBP_BattleHUD
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

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
and cross-Record Skip cleanup.

## R8 Manual PIE Validation — PASS / STICKY

The corrected minimal R8 PIE on
`/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native` accepted Hand-to-PlayArea-to-
Discard, Exhaust disappearance, end-turn/manual Hand discard and strict one-card-at-
a-time DrawPile-to-Hand presentation, with correct final Hand/HUD state and no
flashback, duplicate, transient leak, abnormal HUD, or permanent Input Lock.

The P1 cleanup fix changed only abandoned/Skip cleanup, so this normal visual Gate
remained sticky and did not require another PIE run.

```text
R8 COMPLETE / VALIDATED
R9 NOT STARTED
```

Detailed final R8 evidence is in:

```text
docs/R8NativeCardLifecycleValidation.md
```

## Next Exact Action — STOP

Wait for explicit user authorization before starting R9. Do not enter R9 or any
later phase automatically.

## Blockers

No R8 blocker remains. R9 and all later phases remain NOT STARTED.
