# Phase 6UI-A1 Combatant Inspection and Target Presentation

Status:

```text
COMPLETE
UE5.8 BUILD / AUTOMATION REVALIDATED
UMG SAVED WIRING STRUCTURE REVIEWED
PIE REVALIDATED
```

## Runtime boundary

Combatants now expose presentation metadata:

```text
ACombatant.PresentationId
ACombatant.DisplayName
        ↓ coherent read snapshot
FBattleHUDCombatantView
        ↓
WBP_CombatantPresentation
```

`PresentationId` is a stable presentation-mapping key. It is not authoritative
gameplay identity, a combat ordering key or a legal-target token. Public
`FBattleHUDTargetView` entries carry the same `PresentationId` only so the View
can place a gameplay-provided `TargetId` on the correct visible combatant.

`UBattleHUDCombatantPresentationWidgetBase` is presentation-only. It stores the
latest coherent combatant view and publishes:

```text
OnInspectRequested
OnInspectCleared
OnTargetRequested(TargetId)
```

`OnInspectPinRequested` remains available only as a deferred optional
touch/accessibility hook. The current mouse policy does not bind it and normal
primary click never requests pinned inspection.

It never queries target legality and never calls a gameplay Request. The owning
Battle HUD supplies `bTargetSelectionActive`, `bLegalTarget` and the matching
gameplay-provided `TargetId` on every ViewModel refresh. Visual highlighting is
supplied separately through `bTargetHighlighted`; highlighting alone never makes
a combatant clickable.

While pointer/focus inspection remains active, `SetPresentationData` republishes
the latest view. A stationary pointer therefore cannot leave an open inspector
showing an older gameplay revision.

## Validated level combatant metadata

Current `L_BattleTest` combatant presentation metadata is authored with unique
presentation IDs and player-facing display names. The durable rule is:

```text
Player
Combat | Presentation | Presentation Id = unique stable ID
Combat | Presentation | Display Name    = localized player-facing name

Enemy
Combat | Presentation | Presentation Id = unique stable ID
Combat | Presentation | Display Name    = localized player-facing name
```

IDs must be unique inside the battle. Player-facing localized text belongs in
`DisplayName`; do not use localized display text as `PresentationId`.

## Validated WBP_CombatantPresentation structure

Current asset:

```text
Content/SlayTheSpireDemo/UI/Widgets/WBP_CombatantPresentation
Parent class = BattleHUDCombatantPresentationWidgetBase
```

Recommended/validated hierarchy concept:

```text
Overlay_Root
├── Img_Character                 Not Hit-Testable
├── Btn_Interaction               transparent, Is Focusable = true
└── Border_TargetHighlight        Hit Test Invisible
```

Keep `Btn_Interaction` as a sibling hit layer rather than making the character
Image the Button child. This separates visual sizing/padding from interaction.
Do not disable the Button for an illegal target: it must remain inspectable.

Button event contract:

```text
Btn_Interaction.OnHovered
→ SetPointerInspectionActive(true)

Btn_Interaction.OnUnhovered
→ SetPointerInspectionActive(false)

Btn_Interaction.OnClicked
→ if bTargetSelectionActive && bLegalTarget
    RequestPrimaryInteraction()
  else
    no-op
```

Keyboard/gamepad focus is observed automatically when the focusable Button enters
or leaves this UserWidget's focus path.

`Event Combatant Presentation Changed` updates:

```text
CombatantView
→ Break Battle HUD Combatant View
→ update DisplayName / HP / MaxHP / Block / Statuses

bTargetHighlighted
→ Border_TargetHighlight.Visibility
```

The character portrait may remain a Blueprint instance-editable texture because
it is presentation content and is not part of target legality.

## Validated combatant inspector

`WBP_StatusTooltip` remains the reusable list that owns the individual
status/rules explanation entries. It is not the whole combatant inspector. Its
narrow function remains conceptually:

```text
RebuildStatuses
Input: Statuses (Battle HUD Status View Array)
→ clear VB_StatusEntries
→ create WBP_StatusTooltipEntry for each item
```

`WBP_CombatantTooltip` composes the list:

```text
WBP_CombatantTooltip
└── VB_Root
    ├── Txt_CombatantName
    ├── optional HP / Block text
    └── StatusTooltipList : WBP_StatusTooltip
```

Its `SetCombatantView` path follows:

```text
Break CombatantView
├── DisplayName → Txt_CombatantName.SetText
├── HP / MaxHP  → optional HP text
├── Block       → optional Block text
└── Statuses
    → StatusTooltipList.RebuildStatuses
```

The panel starts `Collapsed`. When visible but not interactive, use
`Not Hit-Testable (Self & All Children)` so it cannot steal hover from the
combatant hit area.

## Validated WBP_BattleHUD ownership

Player and Enemy instances of `WBP_CombatantPresentation` are owned by the Battle
HUD and bind each presentation instance's transient inspect/clear and
target-request dispatchers. `OnInspectPinRequested` remains unbound for the
current hover-only mouse policy.

Inspection handlers:

```text
OnInspectRequested(Presentation)
→ if ViewModel.InteractionState == Idle
    show latest DisplayName
    rebuild/show latest Status details when non-empty
  else if InteractionState is ChoosingTarget or ReadyToConfirm
    show latest DisplayName only
    collapse Status details
  else
    hide DisplayName and Status details

OnInspectCleared(Presentation)
→ hide DisplayName and Status details

OnTargetRequested(TargetId)
→ WBP_BattleHUD.SelectTarget(TargetId)
```

A successful legal-target click clears the current transient inspection before
the synchronous target request is broadcast. Its resulting StateRevision refresh
must not reopen inspection while the pointer remains stationary. Inspection may
open again only after pointer/focus leaves and becomes active again.

The ViewModel exposes a presentation-only lookup over its current public legal set:

```text
TryGetLegalTargetByPresentationId(CombatantView.PresentationId)
→ Found
→ TargetView
```

This lookup does not grant gameplay permission. It only removes target-array
iteration from Blueprint; the formal Request still revalidates authoritatively.

Validated Blueprint helper concept:

```text
RefreshOneCombatantPresentation(PresentationWidget, CombatantView)

CombatantView.PresentationId
→ ViewModel.TryGetLegalTargetByPresentationId
→ Found / TargetView.TargetId

ViewModel.InteractionState == ChoosingTarget
→ bChoosingTarget

SetPresentationData(
    Target                 = PresentationWidget,
    CombatantView          = CombatantView,
    TargetSelectionActive  = bChoosingTarget,
    LegalTarget            = Found,
    TargetId               = Found ? TargetView.TargetId : -1,
    TargetHighlighted      = bChoosingTarget && Found
)
```

Self-target cards use the same character-bound target path as Enemy-target cards.
The public target still originates from Gameplay; clicking the Player emits its
gameplay-provided TargetId and the formal Request revalidates it.

The owning HUD refresh is:

```text
RefreshOneCombatantPresentation(Combatant_PlayerPresentation, ViewModel.Player)
→ RefreshOneCombatantPresentation(Combatant_EnemyPresentation, ViewModel.Enemy)
```

Never use a fixed `SelectTarget(1)` or assume the first legal target is the visible
Enemy. Future `EnemyPresentation[]` instances use the same ID match.

The old separate `VB_LegalTargets` has been removed from the saved HUD Designer.
Its former Sequence `Then 12` entry is disconnected. Historical target-button
nodes remain as unreachable graph nodes only; do not reconnect them. They may be
deleted later as graph cleanup.

Saved UMG structural review on 2026-08-21 confirmed:

```text
Sequence Then 14
→ RefreshCombatantPresentations
→ RefreshOneCombatantPresentation(PlayerPresentation, ViewModel.Player)
→ RefreshOneCombatantPresentation(EnemyPresentation, ViewModel.Enemy)

PlayerPresentation.OnTargetRequested(TargetId)
→ SelectTarget(TargetId)

EnemyPresentation.OnTargetRequested(TargetId)
→ SelectTarget(TargetId)
```

The later owner-run UE5.8 Automation and manual PIE revalidation confirmed the current saved wiring after the Self-target interaction-policy change.

## PIE acceptance — PASSED

Validated without gameplay-driving debug keys:

```text
hover Player / Enemy → inspector shows correct name and latest Status descriptions
keyboard/gamepad focus → same inspector path
normal click outside target selection → no-op; never pins inspection
card selected → hover/focus may show the combatant name, but Status details stay collapsed
select Strike → only legal Enemy presentation highlights
click legal Enemy → inspection clears, then formal SelectTarget(TargetId) resolves the card
select Defend / another Self-target card → only Player presentation highlights
click legal Player → formal SelectTarget(TargetId) resolves the card
status changes while pointer remains still → open inspector refreshes
terminal/resolving state → no stale legal target remains active
```

The source/UMG interaction path is now revalidated. This document no longer blocks UI-A2; the next implementation slice is Basic Committed Presentation.
