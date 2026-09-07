# Card Expansion — Wave 1C-C0 Description Compatibility Amendment

Date: **2026-09-08**

Status:

```text
NARROW COMPATIBILITY AMENDMENT / EFFECTIVE
```

This amendment supersedes only the `DescriptionArgumentName is not None` requirement in section 15 of `CardExpansionWave1CC0SelectExhaustGeneralization.md`.

## Why the amendment is required

The sealed Burning Pact asset already exists on `main` and its current description hard-codes the single-card wording rather than referencing a new `{Exhaust}` argument.

The existing card-text validator deliberately rejects an Effect that declares a dynamic argument which the card description does not use. Therefore making a newly-added `DescriptionArgumentName = Exhaust` mandatory by default would invalidate the existing Burning Pact asset unless that binary asset were resaved.

C0 must not require a binary migration merely to add an optional authored text value.

## Amended contract

`USelectExhaustHandCardEffect` exposes:

```text
DescriptionArgumentName
```

with the following semantics:

```text
DescriptionArgumentName == None
→ explicit dynamic-text opt-out / legacy-compatible mode
→ GetPreviewArgumentNames declares nothing
→ BuildPreviewArguments contributes nothing
→ valid configuration

DescriptionArgumentName != None
→ declare that semantic argument
→ Base card contributes BaseSelectionCount
→ Upgraded card contributes UpgradedSelectionCount
→ existing card-level text validation requires the description template to use the declared argument
```

For new Blueprint-authored cards that want dynamic count text, the intended configuration remains:

```text
DescriptionArgumentName = Exhaust
Description = "Exhaust {Exhaust} cards."
```

This does not introduce sentinel semantics for gameplay values. `BaseSelectionCount` and `UpgradedSelectionCount` remain explicit authored gameplay values exactly as locked by C0.

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

Existing focused Automation coverage already asserts the Player canonical order and Random canonical Exhaust order. These tests still require local execution before C0 can be marked validated or sealed.

## Binary scope

This amendment does not authorize or require changes to:

```text
Content/SlayTheSpireDemo/Data/Cards/Ironclad/Skills/DA_Card_BurningPact.uasset
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```
