# Phase 6UI-A1 Combatant Inspection and Target Presentation

Status:

```text
SOURCE CHANGED
UE5.8 BUILD / AUTOMATION REVALIDATION PENDING
UMG ASSET AUTHORING REQUIRED
PIE REVALIDATION REQUIRED
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
gameplay-provided `TargetId` on every ViewModel refresh.

While pointer/focus inspection remains active, `SetPresentationData` republishes
the latest view. A stationary pointer therefore cannot leave an open inspector
showing an older gameplay revision.

## USER ACTION REQUIRED — level combatant metadata

After compiling the new source, open:

```text
Content/SlayTheSpireDemo/Maps/L_BattleTest
```

Select the Player and Enemy `ACombatant` instances and configure unique values:

```text
Player
Combat | Presentation | Presentation Id = PlayerHero
Combat | Presentation | Display Name    = 铁甲战士

Enemy
Combat | Presentation | Presentation Id = EnemyCultist01
Combat | Presentation | Display Name    = 邪教徒
```

IDs must be unique inside the battle. Player-facing localized text belongs in
`DisplayName`; do not use localized display text as `PresentationId`.

## USER ACTION REQUIRED — WBP_CombatantPresentation

Create:

```text
Content/SlayTheSpireDemo/UI/Widgets/WBP_CombatantPresentation
Parent class = BattleHUDCombatantPresentationWidgetBase
```

Suggested hierarchy:

```text
Overlay_Root
├── Img_Character                 Not Hit-Testable
├── Btn_Interaction               transparent, Is Focusable = true
└── Border_TargetHighlight        Hit Test Invisible
```

Keep `Btn_Interaction` as a sibling hit layer rather than making the character
Image the Button child. This separates visual sizing/padding from interaction.
Do not disable the Button for an illegal target: it must remain inspectable.

Wire Button events:

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

Implement `Event Combatant Presentation Changed`:

```text
CombatantView
→ Break Battle HUD Combatant View
→ update DisplayName / HP / MaxHP / Block / Statuses

bTargetSelectionActive && bLegalTarget
→ Border_TargetHighlight.Visibility
```

The character portrait may remain a Blueprint instance-editable texture because
it is presentation content and is not part of target legality.

## USER ACTION REQUIRED — Combatant inspector

Keep `WBP_StatusTooltip` as the reusable list that owns the individual
status/rules explanation entries. Do not repurpose it as the whole combatant
inspector. Its narrow function remains conceptually:

```text
RebuildStatuses
Input: Statuses (Battle HUD Status View Array)
→ clear VB_StatusEntries
→ create WBP_StatusTooltipEntry for each item
```

Create a separate `WBP_CombatantTooltip` that composes the list:

```text
WBP_CombatantTooltip
└── VB_Root
    ├── Txt_CombatantName
    ├── optional HP / Block text
    └── StatusTooltipList : WBP_StatusTooltip
```

Add this function to `WBP_CombatantTooltip`:

```text
SetCombatantView
Input: CombatantView (Battle HUD Combatant View)
```

Inside the function:

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

## USER ACTION REQUIRED — WBP_BattleHUD ownership

Add Player and Enemy instances of `WBP_CombatantPresentation`. Bind each
presentation instance's transient inspect/clear and target-request dispatchers.
Do not bind `OnInspectPinRequested` for the current hover-only mouse policy.

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

On every `Battle HUD View Model Changed`, configure the Player presentation:

```text
SetPresentationData(
    CombatantView          = ViewModel.Player,
    TargetSelectionActive  = ViewModel.InteractionState == ChoosingTarget,
    LegalTarget            = false,
    TargetId               = -1
)
```

Self-target cards remain confirmation-based and do not expose Player as a public
target button.

For each Enemy presentation, derive legal-target data from the current public
legal set:

```text
bEnemyLegal = false
EnemyTargetId = -1

ForEach ViewModel.LegalTargets
    if LegalTarget.PresentationId == EnemyView.PresentationId
        bEnemyLegal = true
        EnemyTargetId = LegalTarget.TargetId

SetPresentationData(
    CombatantView          = EnemyView,
    TargetSelectionActive  = ViewModel.InteractionState == ChoosingTarget,
    LegalTarget            = bEnemyLegal,
    TargetId               = EnemyTargetId
)
```

Never use a fixed `SelectTarget(1)` or assume the first legal target is the visible
Enemy. Future `EnemyPresentation[]` instances use the same ID match.

The old separate `VB_LegalTargets` can remain temporarily while wiring is checked,
but should be removed/collapsed after character-bound target selection passes PIE
so two target affordances are not shown simultaneously.

## PIE acceptance

Validate without gameplay-driving debug keys:

```text
hover Player / Enemy → inspector shows correct name and latest Status descriptions
keyboard/gamepad focus → same inspector path
normal click outside target selection → no-op; never pins inspection
card selected → hover/focus may show the combatant name, but Status details stay collapsed
select Strike → only legal Enemy presentation highlights
click legal Enemy → inspection clears, then formal SelectTarget(TargetId) resolves the card
select Defend → no Player target highlight; explicit Confirm remains required
status changes while pointer remains still → open inspector refreshes
terminal/resolving state → no stale legal target remains active
```

Do not mark this source addition validated until the owner recompiles and the
relevant automation/PIE checks pass.
