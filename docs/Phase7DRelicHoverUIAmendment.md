# Phase 7D Relic Hover UI Amendment

Status: **COMPLETE / VALIDATED / SEALED**

This amendment refines only the Native Phase 7D Relic presentation described by `docs/Phase7RelicsImplementation.md`. Gameplay, Relic runtime state, trigger behavior, Read/Frozen DTOs and FinalSnapshot reconciliation remain unchanged.

## Visible contract

The steady-state Relic strip follows the Slay-the-Spire-style compact presentation:

```text
Relic with no visible counter
→ icon only

Relic with visible counter
→ icon + current Counter as one integer badge at the icon's lower-right
→ examples: 0, 1, 2
→ never render 0/3, 1/3, 2/3 on the steady-state icon
```

`bShowCounter` remains the generic data-driven switch. `CounterMax` remains frozen presentation metadata for future progress-ring/bar/tooltip use, but the current badge does not render it. UI must not special-case `RelicId` to decide counter visibility or formatting.

## Hover tooltip contract

Hovering a Relic icon creates one transient Native tooltip that follows the mouse cursor with a small offset:

```text
Relic interaction hit layer hover
→ transient tooltip
   - frozen DisplayName
   - frozen Description
→ mouse move while hovered
   - tooltip follows cursor
→ mouse leave / Relic Widget destruction
   - tooltip is removed
```

The tooltip is `HitTestInvisible` so it cannot steal hover ownership from the Relic interaction hit layer. No standard UMG `SetToolTipText` path is used in parallel.

The tooltip consumes only `FBattleHUDRelicView`. It must not query `URelicInstance`, `URelicData`, `ABattleManager` or any mutable Gameplay state.

## Explicit interaction hit layer

Relic hover uses the same validated interaction pattern as `WBP_CombatantPresentation`: a transparent Button is an explicit sibling hit layer instead of relying on `UUserWidget::NativeOnMouseEnter`.

```text
WBP_BattleRelic_Native
└─ Overlay_Root
   ├─ Img_RelicIcon             Hit Test Invisible
   ├─ Btn_RelicInteraction      transparent; sole hover hit layer
   └─ Border_Counter            Hit Test Invisible
      └─ Txt_RelicCounter       Hit Test Invisible
```

The Button does not own Gameplay interaction and has no click behavior. Its Native bindings are only:

```text
Btn_RelicInteraction.OnHovered
→ UBattleRelicWidget::HandleRelicHovered
→ ShowRelicTooltip

Btn_RelicInteraction.OnUnhovered
→ UBattleRelicWidget::HandleRelicUnhovered
→ HideRelicTooltip
```

The icon/counter are presentation-only siblings and do not participate in Slate hit testing. This prevents visual children from competing with the explicit hover target.

The owning `RelicStrip_Player` in `WBP_BattleHUD_Native` must be `Not Hit-Testable (Self Only)`, not `Not Hit-Testable (Self & All Children)`, so the child interaction Button remains in the Slate hit-test path.

## Native class split

```text
UBattleRelicWidget
→ steady-state icon + optional current-counter badge
→ binds Btn_RelicInteraction hover/unhover lifecycle
→ owns cursor-follow positioning
→ creates UBattleRelicTooltipWidget from an authored TooltipWidgetClass

UBattleRelicTooltipWidget
→ frozen DisplayName + Description only
→ no Gameplay query or mutation

UBattleRelicStripWidget
→ unchanged identity/order ownership
→ continues to reuse Relic widgets by (RelicId, RuntimeSequence)
```

## Required Designer bindings

`WBP_BattleRelic_Native : UBattleRelicWidget`

```text
Btn_RelicInteraction : Button
Img_RelicIcon        : Image
Txt_RelicCounter     : TextBlock
```

`Txt_RelicName` is no longer a steady-state binding.

`WBP_BattleRelicTooltip_Native : UBattleRelicTooltipWidget`

```text
Txt_RelicName         : TextBlock
Txt_RelicDescription  : TextBlock
```

`WBP_BattleRelicStrip_Native : UBattleRelicStripWidget` remains:

```text
HB_Relics : HorizontalBox
```

## Validation evidence

C++ gate:

```text
Development Editor Build                                      PASS
SlayTheSpireDemo.Phase7.RelicPresentation                     3/3 PASS
```

Focused PIE acceptance on 2026-09-03:

```text
steady state shows icon only (+ optional single-number badge)  PASS
hover shows one custom tooltip with name + description         PASS
mouse movement moves the tooltip                               PASS
mouse leave removes it                                         PASS
no duplicate / stuck tooltip                                   PASS
Sundial badge displays 0 -> 1 -> 2 -> 0, never /3             PASS
third-shuffle A2 playback retains historical 2                 PASS
same Envelope FinalSnapshot reconciles visible counter to 0    PASS
```

## Seal

This amendment is complete, validated and sealed as part of Phase 7D. Future cosmetic Relic artwork changes, including optional outline presentation, do not reopen this contract.
