# Phase 6UI-A1 — Operable Battle HUD

Status: **SOURCE CHANGED / UE5.8 REVALIDATION + UMG ASSET + PIE VALIDATION PENDING**.

UI-A1 turns the completed UI-A0 gameplay/read boundary into a presentation-facing, operable HUD architecture. Runtime C++ provides a Blueprint-friendly ViewModel, a UMG base widget contract and a presentation-only scene presenter. Text tooling does not create or edit `.uasset` / `.umap`; the concrete Widget Blueprint and level assembly remain explicit user actions.

The combatant inspection/character-bound target extension additionally provides
`UBattleHUDCombatantPresentationWidgetBase`, stable presentation-only combatant
IDs and data-authored combatant display names. Exact UMG assembly steps live in
`docs/Phase6UIA1CombatantInspectionSetup.md`.

The owner-confirmed UE5.8 source gate previously passed before the later CardType/Description presentation extension and the Self-target interaction-policy revision:

```text
SlayTheSpireDemoEditor build  PASS
Phase5       13/13 PASS
Phase6A      23/23 PASS
Phase6B      12/12 PASS
Phase6C       5/5  PASS
Phase6UIA0   20/20 PASS
Phase6UIA1    9/9  PASS
Previous run 82/82 PASS
```

That run remains historical evidence only. The later card-presentation fields and Self-target interaction-policy changes require a fresh UE5.8 workflow run. The exact total is run evidence, not a permanent architecture acceptance constant.

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
Combatant PresentationId / data-authored DisplayName
Energy / MaxEnergy
Hand card views, including DisplayName / Cost / CardType / Description
Draw / Discard / Exhaust counts
committed Enemy Intent presentation
CurrentResolvedDamageAmount
selection state / legal target presentation
player-facing rejection feedback
Victory / Defeat / ResolutionFaulted outcome
```

Card presentation retains `RuntimeId` as the stable UI identity for the current battle. The ViewModel internally keeps weak runtime card references only to forward a later formal Request; Request revalidation remains authoritative.

Card identity/art fields come from the card definition. Description now arrives as
a gameplay-derived, player-facing snapshot value rather than Widget-side parsing:

```text
UCardData.Description format
+ CardEffect named preview values
+ read-only Modifier Pipeline
        ↓ TryBuildPlayerFacingReadSnapshot
FCardReadView.CurrentDescription
        ↓
UBattleHUDViewModel::RebuildHandViews
        ↓
FBattleHUDCardView
        ↓
WBP_BattleCard
```

UMG does not derive player-facing rules text or inspect `CardEffect` objects. The
minimal resolver added for UI-A3 produces final FText before the ViewModel boundary.

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

Target interaction is determined by `ECardTargetType`, not by `ECardType`:

```text
Enemy
→ GetLegalTargetsForCard at selection time
→ expose presentation LegalTargets
→ ChoosingTarget
→ player chooses one candidate
→ RequestPlayCard revalidates authoritatively at submission time

Self
→ GetLegalTargetsForCard at selection time
→ expose the gameplay-provided Player through public LegalTargets
→ ChoosingTarget
→ player selects the Player presentation
→ SelectTargetById submits the selected Player candidate
→ RequestPlayCard revalidates authoritatively at submission time

None
→ no target candidate
→ ReadyToConfirm
→ ConfirmSelectedCard submits nullptr
→ RequestPlayCard revalidates authoritatively at submission time
```

Public `LegalTargets` are advisory presentation candidates, not capability tokens. They include the Player for Self-target cards and enemies for Enemy-target cards. The final `RequestPlayCard` always re-runs gameplay-owned validation against current authoritative state. Only no-target cards use `ConfirmSelectedCard()`.

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

The Runtime module depends on `UMG` because `UBattleHUDWidgetBase` is a public Runtime type. The project still does not enable the Unreal MVVM plugin; the architecture uses MVVM-style responsibility boundaries without adding a plugin dependency.

## Automation source gate — REVALIDATION PENDING

Editor-only UI-A1 tests live under:

```text
Source/SlayTheSpireDemoTests/Private/Phase6UIA1TestFixture.h
Source/SlayTheSpireDemoTests/Private/Phase6UIA1ViewModelTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA1InteractionTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase6UIA1TerminalTests.cpp
```

Current named invariants include:

```text
ViewModel.SubscribeThenPullBuildsHUD
ViewModel.CardPresentationFieldsComeFromDefinition
ViewModel.SelectionUsesLegalTargetsAndCancelIsPresentationOnly
ViewModel.NoTargetRequiresConfirmAndLocksUntilReady
ViewModel.SelfTargetUsesLegalPlayerSelection
ViewModel.TargetRequestLocksUntilReady
ViewModel.EndTurnLocksUntilReady
ViewModel.UnplayableCardSurfacesGameplayReason
ViewModel.ReadyRefreshClearsStaleSelection
ViewModel.IntentUsesGameplayDerivedCurrentValue
ViewModel.ResolutionFaultIsVisibleTerminalState
```

The Self-target regression uses a real `UGainBlockCardEffect` configured before `StartBattle()` so the shared `UCardData` definition is not mutated after runtime initialization. It validates the complete Defend-style path:

```text
Self card selected
→ SelectedCardRuntimeId remains selected
→ public LegalTargets contains the gameplay-provided Player
→ ChoosingTarget
→ SelectTargetById receives the Player target ID
→ gameplay Request receives the selected Player candidate
→ authoritative Request revalidation
→ GainBlockAction resolves
→ Energy is spent
→ card leaves Hand and reaches Discard
→ stable Ready clears selection/public target state
```

The owner-only workflow is:

```text
.github/workflows/ue-phase6uia1-tests.yml
```

The workflow continues to expect 11 Phase6UIA1 tests and now additionally expects
8 Phase6UIA3 dynamic-text tests, for 92 discovered tests across the gated prefixes.
These exact discovered counts are an operational missing-test guard; the durable
acceptance rule is not a permanent numeric total:

```text
UE5.8 Editor build passes
+
all existing Phase5 / Phase6 / UI-A0 regressions pass
+
all currently named UI-A1 ViewModel invariants pass
+
concrete WBP_BattleHUD can be assembled and PIE-validates one normal playable battle loop
```

After the CardType/Description extension and Self-target interaction-policy revision, the first three source requirements require a fresh owner-run workflow before they can be claimed again. UI-A1 also remains incomplete until the concrete UMG/PIE requirement passes.

## User asset work — CURRENT NEXT STEP

User action is required in UE Editor:

```text
create WBP_BattleHUD derived from UBattleHUDWidgetBase
create WBP_CombatantPresentation derived from UBattleHUDCombatantPresentationWidgetBase
map public LegalTargets to combatant presentations by PresentationId
use transient hover/focus combatant inspection; normal click never pins it
build minimum HP / Block / Energy / Status / Hand / pile / Intent / End Turn surface
build WBP_BattleCard and bind DisplayName / Cost / CardType / Description from FBattleHUDCardView
wire card selection / cancel / target / confirm / End Turn through base-widget methods
place ABattleHUDPresenter in L_BattleTest
assign the existing BattleManager instance
assign WBP_BattleHUD as WidgetClass
PIE one player → enemy → player cycle without gameplay-driving debug keys
```

The current Self-target PIE acceptance case is explicit:

```text
Defend
→ select card
→ ChoosingTarget
→ Player presentation receives the legal target ID and four-corner highlight
→ click Player
→ Player gains the configured Block
→ Energy is spent
→ card reaches its resolved destination
```

Enemy-target PIE remains:

```text
Strike / Pommel Strike
→ select card
→ ChoosingTarget
→ choose gameplay-provided Enemy target
→ damage resolves
```

Existing binary CardData assets must have their new `Description` field authored in UE Editor; text tooling does not rewrite `.uasset` contents.

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
