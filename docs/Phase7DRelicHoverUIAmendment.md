# Phase 7D Relic Hover UI Amendment

Status: **IMPLEMENTATION AUTHORIZED / C++ VALIDATION PENDING**

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
Relic icon hover
→ transient tooltip
   - frozen DisplayName
   - frozen Description
→ mouse move while hovered
   - tooltip follows cursor
→ mouse leave / Relic Widget destruction
   - tooltip is removed
```

The tooltip is `HitTestInvisible` so it cannot steal hover ownership from the Relic icon. No standard UMG `SetToolTipText` path is used in parallel.

The tooltip consumes only `FBattleHUDRelicView`. It must not query `URelicInstance`, `URelicData`, `ABattleManager` or any mutable Gameplay state.

## Native class split

```text
UBattleRelicWidget
→ steady-state icon + optional current-counter badge
→ owns hover lifecycle and cursor-follow positioning
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
Img_RelicIcon       : Image
Txt_RelicCounter    : TextBlock
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

## Validation boundary

C++ gate:

```text
regenerate project files
Development Editor Build
SlayTheSpireDemo.Phase7.RelicPresentation remains PASS
```

The new hover behavior is a visual/input presentation requirement and therefore requires one focused PIE after the Native WBP assets are created:

```text
steady state shows icon only (+ optional single-number badge)
hover shows one custom tooltip with name + description
mouse movement moves the tooltip
mouse leave removes it
Sundial badge displays 0 -> 1 -> 2 -> 0, never /3
third-shuffle EnergyChanged playback preserves the already-sealed FinalSnapshot timing contract
```
