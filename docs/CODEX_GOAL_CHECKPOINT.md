# Codex Goal Checkpoint — Phase 6UI-A2N

Last updated: **2026-08-31**

## Goal

Migrate the sealed Legacy HUD behavior to the Native HUD stack under
`docs/Phase6UIA2NNativeHUDRefactor.md`, without changing Gameplay authority,
Presentation Record/Envelope semantics, Controller/reducer ownership, or UI-A3.

This file is an execution checkpoint, not the full historical evidence archive.
Detailed prior evidence remains in:

```text
docs/UIA2NNativeHUDBaseline.md
docs/Validation.md
docs/R3AReviewFixValidation.md
docs/R4NativeCardHandValidation.md
```

## Current Status

```text
Goal execution status:
IN PROGRESS — R0 / R1 / R2 / R3-A / R4 COMPLETE AND VALIDATED; R5+ NOT STARTED

Base branch: main
R4 working branch: a2n/r4-native-card-hand
R4 base main: 9981dcebda27ae5be46be608177084412e78b1fb

Production map:
/Game/SlayTheSpireDemo/Maps/L_BattleTest

Production WidgetClass:
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD_C

Native test map:
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native

Native test WidgetClass:
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
```

Production remains Legacy. The Native stack remains the migration/test path until the separately defined production cutover phase.

## Sealed Legacy Baseline

```text
UI-A2E implementation:
81cbfb6af09a52f96ececff597491c5bfcc3665f

UI-A2E seal docs:
666025c4cc6af2dc1ecf22c51f23810fd8892bb3

WBP_BattleHUD:
990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A

WBP_BattleCard:
1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F

WBP_BattleStatus:
205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

Legacy assets remain frozen during A2N migration.

## Completed Stages

### R0 — Baseline and Native test injection

**COMPLETE / VALIDATED**

Key results:

```text
75 HUD Designer Widgets inventoried
23 Required BindWidget
6 BindWidgetOptional
46 Designer-only
single Presenter WidgetClass injection retained
production remains Legacy
L_BattleTest_Native selected as isolated Native test path
```

### R1 — Backward-compatible base native hook

**COMPLETE / VALIDATED**

Implementation commit:

```text
496224de8fa549e7ac3563adf04e58743f072b85
refactor(ui-a2n): add native HUD refresh hook
```

The Legacy default still routes through `BP_OnViewModelChanged`; Native concrete HUD overrides the native hook without calling Super. Cancellation suppression and exact-token fail-safe behavior remain intact.

### R2 — Native HUD shell

**COMPLETE / VALIDATED**

Implementation commit:

```text
d15287ec068f699390a4f64cfab824dcbe53980b
refactor(ui-a2n): add native HUD shell
```

Created the Designer-backed Native HUD/Card/Status shells and `L_BattleTest_Native`. Production and Legacy assets remained unchanged.

### R3-A — Static HUD and long-lived delegates

**COMPLETE / VALIDATED**

Accepted ownership:

```text
RefreshCombatants
RefreshEnergy
RefreshPileCounts
RefreshInputState
RefreshFeedback
RefreshEnemyIntent
RefreshTerminalFromViewModel
EndTurn / Confirm / Cancel
Combatant target / inspect delegates
```

R3 review-fix focused suite:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge                 PASS
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip              PASS
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal                   PASS
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable    PASS

4/4 PASS
```

This closes zero-Block badge visibility, frozen Status tooltip parity, Terminal ViewModel rendering, and PresentationUnavailable separation.

### R4 — Native Card Widget, Hand and Card input

**COMPLETE / VALIDATED**

Implemented only the formal Hand/Card ownership boundary:

```text
UBattleCardWidget
  SetCardView
  GetRuntimeId
  GetCardId
  GetCardView
  OnBattleCardRequested(RuntimeId)

UBattleHUDWidget
  RefreshHand
  formal Hand Widget creation
  one request binding per created Card Widget
  exact RuntimeId -> UBattleHUDWidgetBase::SelectCard
```

Card Widget owns only its supplied `FBattleHUDCardView`. It has no HUD, ViewModel, Gameplay card, BattleManager, Controller, Record or Envelope reference.

The user completed the requested UE5.8 R4 acceptance gate on the saved branch:

```text
SlayTheSpireDemoEditor Win64 Development build: PASS
WBP_BattleCard_Native compile: PASS
WBP_BattleHUD_Native compile: PASS
SlayTheSpireDemo.Phase6UIA2N.R4 focused Automation: PASS
L_BattleTest_Native Hand rebuild/display parity: PASS
formal SelectCard(RuntimeId) input path: PASS
ChoosingTarget / target highlight / Cancel: PASS
accepted legal-target request occurs once: PASS
no duplicate dynamic card callback: PASS
R3 zero-Block behavior remains correct: PASS
production / Legacy boundary unchanged: PASS
```

Formal cards continue through ViewModel/Gameplay authority for playability and target validation. R4 does not make presentation-only cards interactive.

Detailed R4 evidence:

```text
docs/R4NativeCardHandValidation.md
```

## Explicitly Unmigrated After R4

The following remain outside R4:

```text
CardPlayed committed visual playback
CardZoneChanged committed visual playback
Native playback token/timer kernel
PlayArea presentation-only transient lifecycle
Damage presentation
BlockChanged / EnergyChanged / DeckShuffled playback
formal Status-row lifecycle
Terminal Record sequencing
Controller / Reducer / Record / Envelope changes
production cutover
UI-A3 Preview
```

`UBattleHUDWidget::BeginPresentationRecordPlayback_Implementation` still returns `false`, preserving immediate fallback for all Records.

## Next Exact Action — R5 Native Playback Kernel

R5 is the next stage, but it is **NOT STARTED**.

R5 must establish only the native playback ownership kernel:

```text
exact active Token
Record type
visual finish timer
local presentation-only transient references
valid Begin / false-with-zero-side-effects
exact-token Finish
exact-token Cancel
Widget destruction local cleanup
```

Do not begin R6/R7/R8/R9 Record-specific migration as part of the R5 kernel unless the dedicated phase plan explicitly requires it.

## Blockers

```text
R4 blockers: none
R5+: NOT STARTED
production cutover: not authorized / not reached
Legacy destructive cleanup: not authorized
```
