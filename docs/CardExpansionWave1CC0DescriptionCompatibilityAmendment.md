# Card Expansion — Wave 1C-C0 Description Compatibility Amendment

Date: **2026-09-08**

Status:

```text
NARROW COMPATIBILITY AMENDMENT / EFFECTIVE
```

This amendment supersedes only the dynamic-description requirements in section 15 of `CardExpansionWave1CC0SelectExhaustGeneralization.md`.

## Why the amendment is required

The sealed Burning Pact asset already exists on `main` and its current description hard-codes the single-card wording rather than referencing new dynamic Select-Exhaust arguments.

The existing card-text validator deliberately rejects an Effect that declares a dynamic argument which the card description does not use. Therefore making newly-added description arguments mandatory by default would invalidate the existing Burning Pact asset unless that binary asset were resaved.

C0 must not require a binary migration merely to add optional authored text values.

## Amended contract

`USelectExhaustHandCardEffect` exposes two optional dynamic-description argument names:

```text
DescriptionArgumentName
SelectionModeDescriptionArgumentName
```

### Count argument

```text
DescriptionArgumentName == None
→ explicit dynamic-count opt-out / legacy-compatible mode
→ no count argument is declared or contributed

DescriptionArgumentName != None
→ declare that semantic argument
→ Base card contributes BaseSelectionCount
→ Upgraded card contributes UpgradedSelectionCount
→ existing card-level text validation requires the description template to use the declared argument
```

### Selection-mode phrase argument

```text
SelectionModeDescriptionArgumentName == None
→ explicit dynamic-mode-text opt-out / legacy-compatible mode
→ no mode-text argument is declared or contributed

SelectionModeDescriptionArgumentName != None
→ declare that semantic argument
→ Base card uses BaseSelectionMode
→ Upgraded card uses UpgradedSelectionMode
→ Random contributes localized text equivalent to "Randomly exhaust"
→ Player contributes localized text equivalent to "Exhaust"
→ existing card-level text validation requires the description template to use the declared argument
```

The two argument names are independent. A card may opt into either one or both. Existing duplicate-description-argument validation still applies, so the two semantic names must not collide when both are authored.

For new Blueprint-authored cards that want count and selection-mode text to change across upgrade, the intended configuration is:

```text
DescriptionArgumentName = Exhaust
SelectionModeDescriptionArgumentName = ExhaustMode
Description = "{ExhaustMode} {Exhaust} card from your hand."
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
Base     → "Randomly exhaust 1 card from your hand."
Upgraded → "Exhaust 1 card from your hand."
```

This does not introduce sentinel semantics for gameplay values. `BaseSelectionMode`, `BaseSelectionCount`, `UpgradedSelectionMode`, and `UpgradedSelectionCount` remain explicit authored gameplay values exactly as locked by C0.

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

Existing focused Automation coverage asserts the Player canonical order and Random canonical Exhaust order. The C0 description coverage also verifies Base/Upgraded count and selection-mode text resolution. These tests require local execution before C0 can be marked validated or sealed.

## Binary scope

This amendment does not authorize or require changes to:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```
