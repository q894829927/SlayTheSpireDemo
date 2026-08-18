# Phase 6UI-A1 — Operable Battle HUD

Status: **SOURCE FOUNDATION IMPLEMENTED / UE5.8 BUILD + AUTOMATION + UMG ASSET/PIE VALIDATION PENDING**.

UI-A1 turns the completed UI-A0 gameplay/read boundary into a presentation-facing, operable HUD architecture. Runtime C++ provides a Blueprint-friendly ViewModel, a UMG base widget contract and a presentation-only scene presenter. Text tooling does not create or edit `.uasset` / `.umap`; the concrete Widget Blueprint and level assembly remain explicit user actions after the source gate is green.

## Responsibility split

```text
Authoritative Gameplay
ABattleManager / Combatants / Deck / Status / Intent / Queue
        ↓
TryBuildPlayerFacingReadSnapshot
        ↓
UBattleHUDViewModel
        ↓
UBattleHUDWidgetBase
        ↓
WBP_BattleHUD (user-created UMG asset)
```

Player commands return only through the formal request boundary:

```text
WBP_BattleHUD
↓
UBattleHUDWidgetBase convenience call
↓
UBattleHUDViewModel
↓
RequestPlayCard / RequestEndPlayerTurn
↓
authoritative validation + ActionQueue resolution
```

`ABattleManager` does not depend on UMG and does not create Widgets.

`ABattleHUDPresenter` is presentation assembly only. It owns the runtime ViewModel/Widget instance, has an explicit `BattleManager` instance reference and a configurable Widget class. It does not discover the BattleManager through world search.

## ViewModel read contract

`UBattleHUDViewModel::Initialize(...)` obeys the UI-A0 durable initialization order:

```text
bind OnReadStateReady
↓
immediately pull TryBuildPlayerFacingReadSnapshot
↓
build initial HUD if readable
↓
consume later Ready edges for future revisions
```

The ViewModel caches display data only. It does not own authoritative HP, Block, Energy, Status, Hand, zone membership, Intent or legality.

Current Blueprint-facing data includes:

```text
BattleId / StateRevision
Player HP / MaxHP / Block / Status
Enemy HP / MaxHP / Block / Status
Energy / MaxEnergy
Hand card views
Draw / Discard / Exhaust counts
committed Enemy Intent presentation
CurrentResolvedDamageAmount
selection state / legal target presentation
player-facing rejection feedback
Victory / Defeat / ResolutionFaulted outcome
```

Card presentation retains `RuntimeId` as the stable UI identity for the current battle. The ViewModel internally keeps weak runtime card references only to forward a later formal Request; Request revalidation remains authoritative.

## Interaction state

UI-A1 uses the intentionally explicit UI-A policy:

```text
Idle
↓ SelectCard
ChoosingTarget
or ReadyToConfirm
↓ SelectTarget / Confirm
Request accepted
↓
Resolving
↓ OnReadStateReady
Idle / Terminal
```

Reselecting the same card may cancel selection through the ViewModel, and `CancelSelection()` is also exposed explicitly.

Enemy/Self target selection uses `ABattleManager::GetLegalTargetsForCard(...)`; Widget code receives presentation target IDs and never hard-codes `BattleManager.Enemy` as its interaction rule.

No-target cards require `ConfirmSelectedCard()` in this first UI slice. This is presentation policy only and does not alter card gameplay APIs.

## Resolving lock

The ViewModel relies on the UI-A0 no-reentrant completion guarantee:

```text
Request returns AcceptedForResolution
↓
ViewModel commits InteractionState = Resolving
↓
input locked
↓
OnReadStateReady arrives later
↓
pull one coherent snapshot
↓
refresh HUD
↓
release input only if authoritative state is PlayerTurn
```

It does not inspect `OnQueueEmpty`, Queue-level idle, pending Actions or frame timing.

Before UI-A2, presentation catch-up remains immediate/no-op once Ready arrives. There is no Presentation Record playback yet.

## Enemy Intent display

The HUD ViewModel copies both concepts without merging their meanings:

```text
BaseAmount
= committed authoritative Intent plan input

CurrentResolvedDamageAmount
= gameplay-derived damage value for the current snapshot state
```

The ViewModel does not run Strength/Weak/Vulnerable formulas. The current resolved value already comes from the gameplay snapshot pipeline and is not relabeled as guaranteed future damage.

## Failure and terminal presentation

Gameplay rejection is converted to player-facing feedback text in one presentation mapping. The mapping explains the gameplay-owned failure reason; it does not decide legality.

Terminal mapping:

```text
BattleState::Victory           → HUD Outcome::Victory
BattleState::Defeat            → HUD Outcome::Defeat
BattleState::ResolutionFaulted → HUD Outcome::ResolutionFaulted
```

All terminal states lock normal input. `ResolutionFaulted` is therefore visible to the HUD instead of appearing as a silent freeze.

## Runtime presentation classes

```text
Source/SlayTheSpireDemo/UI/BattleHUDTypes.h
Source/SlayTheSpireDemo/UI/BattleHUDViewModel.h/.cpp
Source/SlayTheSpireDemo/UI/BattleHUDWidgetBase.h/.cpp
Source/SlayTheSpireDemo/UI/BattleHUDPresenter.h/.cpp
```

The Runtime module now depends on `UMG` because `UBattleHUDWidgetBase` is a public Runtime type. The project still does not enable the Unreal MVVM plugin; the architecture uses MVVM-style responsibility boundaries without adding a plugin dependency.

## Automation source gate

Editor-only UI-A1 tests live in:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA1RegressionTests.cpp
```

Current named invariants cover:

```text
ViewModel.SubscribeThenPullBuildsHUD
ViewModel.SelectionUsesLegalTargetsAndCancelIsPresentationOnly
ViewModel.NoTargetRequiresConfirmAndLocksUntilReady
ViewModel.TargetRequestLocksUntilReady
ViewModel.EndTurnLocksUntilReady
ViewModel.UnplayableCardSurfacesGameplayReason
ViewModel.ReadyRefreshClearsStaleSelection
ViewModel.IntentUsesGameplayDerivedCurrentValue
ViewModel.ResolutionFaultIsVisibleTerminalState
```

The owner-only workflow is:

```text
.github/workflows/ue-phase6uia1-tests.yml
```

The workflow uses exact discovered counts as an operational missing-test guard. The durable acceptance rule is not a permanent numeric total:

```text
UE5.8 Editor build passes
+
all existing Phase5 / Phase6 / UI-A0 regressions pass
+
all currently named UI-A1 ViewModel invariants pass
+
concrete WBP_BattleHUD can be assembled and PIE-validates one normal playable battle loop
```

## User asset work after source gate

After the C++/Automation gate is green, user action is required in UE Editor:

```text
create WBP_BattleHUD derived from UBattleHUDWidgetBase
build minimum HP / Block / Energy / Status / Hand / pile / Intent / End Turn surface
wire card selection / cancel / target / confirm / End Turn through base-widget methods
place ABattleHUDPresenter in L_BattleTest
assign the existing BattleManager instance
assign WBP_BattleHUD as WidgetClass
PIE one player → enemy → player cycle without gameplay-driving debug keys
```

Exact widget construction steps should be given only after source compilation/Automation is green, so Blueprint work is not built on an unvalidated C++ surface.

## Not in UI-A1

```text
Presentation Records / Presentation Queue
historical damage/draw/status animations
drag and drop
fast play shortcuts
target arrows
card outcome preview
Keyword/CardText system
Relics
final responsive/accessibility polish
```

Those remain UI-A2, UI-A3, later UI-B or Presentation Polish work according to `AGENTS.md`.
