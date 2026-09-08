# Card Expansion — Wave 1C-C0 Description Compatibility Amendment

Date: **2026-09-08**

Status:

```text
NARROW COMPATIBILITY AMENDMENT / EFFECTIVE
```

This amendment supersedes only the dynamic-description requirements in section 15 of `CardExpansionWave1CC0SelectExhaustGeneralization.md`.

## Default contract

`USelectExhaustHandCardEffect` exposes two dynamic-description argument names with semantic defaults:

```text
DescriptionArgumentName = Exhaust
SelectionModeDescriptionArgumentName = ExhaustMode
```

A newly-created Effect therefore starts in a directly usable state and does not require authors to replace `None` before using dynamic description text.

### Count argument

```text
DescriptionArgumentName = Exhaust
→ Base card contributes BaseSelectionCount
→ Upgraded card contributes UpgradedSelectionCount
→ description template references {Exhaust}
```

Authors may explicitly clear `DescriptionArgumentName` to `None` only when intentionally opting out of dynamic count text for legacy content.

### Selection-mode phrase argument

```text
SelectionModeDescriptionArgumentName = ExhaustMode
→ Base card uses BaseSelectionMode
→ Upgraded card uses UpgradedSelectionMode
→ Random contributes localized text "随机消耗"
→ Player contributes localized text "消耗"
→ description template references {ExhaustMode}
```

Authors may explicitly clear `SelectionModeDescriptionArgumentName` to `None` only when intentionally opting out of dynamic mode text for legacy content.

The two semantic names remain independently authorable. Existing duplicate-description-argument validation applies, so they must not collide when both are enabled.

## Intended authoring

For normal new Select-Exhaust content, no manual argument-name setup is required. The intended description template is:

```text
{ExhaustMode}{Exhaust}张牌。
```

Example authored gameplay values:

```text
BaseSelectionMode = Random
BaseSelectionCount = 1
UpgradedSelectionMode = Player
UpgradedSelectionCount = 1
```

resolve as:

```text
Base     → "随机消耗1张牌。"
Upgraded → "消耗1张牌。"
```

This does not introduce sentinel semantics for gameplay values. `BaseSelectionMode`, `BaseSelectionCount`, `UpgradedSelectionMode`, and `UpgradedSelectionCount` remain explicit authored gameplay values exactly as locked by C0.

## Legacy content

Legacy hard-coded descriptions can still opt out deliberately by authoring:

```text
DescriptionArgumentName = None
SelectionModeDescriptionArgumentName = None
```

`None` is therefore an explicit authored compatibility choice, not the default for a newly-created Effect.

The existing Burning Pact binary asset is not modified by this text/code amendment. If that asset is later re-authored to use the semantic defaults, its description template should also be updated in Unreal to reference `{Exhaust}` and `{ExhaustMode}` as appropriate.

## C0-8 stable-order completion note

No new generic SelectionResolver ordering rule is added for C0-8.

Stable order is already enforced at the two exact-N set producers:

```text
Player
→ BattleSelectionRequest canonicalizes submitted RuntimeIds to request candidate order

Random
→ URandomSelectionAction chooses membership without replacement
→ rebuilds FSelectionResult in original candidate order
```

This keeps generic Selection capable of supporting a future consumer whose selected-object order is semantically meaningful, while Select-Exhaust remains set-style and deterministic.

Existing focused Automation coverage asserts the Player canonical order and Random canonical Exhaust order. The C0 description coverage also verifies semantic default argument names plus Base/Upgraded count and selection-mode text resolution. These tests require local execution before C0 can be marked validated or sealed.

## Binary scope

This amendment does not authorize changes to:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```
