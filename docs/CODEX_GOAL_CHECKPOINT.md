# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Expansion / Upgrade Foundation:
REVIEW FIX IMPLEMENTED / REVALIDATION PENDING

Production Card Base/Plus Authoring:
BLOCKED ONLY BY CURRENT DIRECTLY-INVALIDATED REVALIDATION GATES

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED
```

## Current Upgrade Foundation authority

```text
docs/CardUpgradeFoundationDesign.md
docs/CardUpgradeFoundationImplementation.md
docs/CardUpgradeFoundationValidation.md
```

Current authoritative ordinary-card model:

```text
UCardData
├─ stable shared fields
│  ├─ CardId
│  ├─ DisplayName
│  ├─ CardArt
│  ├─ CardType
│  └─ TargetType
│
├─ Base
│  ├─ Description
│  ├─ BaseCost
│  ├─ DefaultDestination
│  └─ Effects[]
│
├─ bHasUpgrade
└─ Upgrade : FCardUpgradeConfig
   ├─ Description
   ├─ Cost
   ├─ DefaultDestination
   └─ Effects[]

UCardInstance
├─ Definition
├─ RuntimeId
└─ bool bUpgraded
```

`UCardVariantData / UpgradedVariant` is no longer part of ordinary-card authoring. A hidden load-only compatibility shim may remain for assets saved during the brief old implementation window, but it is not an authoring surface.

## Stable metadata rule

These values are authored once and are not duplicated in Upgrade:

```text
DisplayName
CardArt
CardType
TargetType
```

The upgraded DisplayName string itself does not change:

```text
Base     → Strike
Upgraded → Strike
```

No auto-appended `+` and no second authored name.

Upgrade state is presented visually:

```text
Base name     → Designer/default color
Upgraded name → gold
```

## Presentation state boundary

The mutable Gameplay flag is frozen before UI consumption:

```text
UCardInstance::IsUpgraded()
→ FPresentationCardSnapshot.bUpgraded
→ PresentationCardView
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ gold Txt_CardName
```

The current Hand freeze also writes the same `bUpgraded` fact directly into `FBattleHUDCardView`.

The Widget never queries Gameplay to decide the color and never mutates upgrade state.

## Effective gameplay boundary

```text
stable:
GetDisplayName()   // same authored text before/after upgrade
GetCardArt()
GetCardType()
GetTargetType()

Base/Upgrade-sensitive:
GetDescriptionFormat()
GetCurrentCost()
ResolveDestination()
GetEffects()
```

Gameplay / Preview consumers do not branch on specific CardId or Effect type to implement ordinary upgrade behavior.

## Mutation authority

```text
UUpgradeCardAction
→ CardInstance::CommitUpgrade
→ false -> true once
```

Second upgrade remains generic fail-soft and does not ResolutionFault.

## Historical validation and current invalidation

Earlier user-side UE5.8 evidence:

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

Those results remain historical/sticky where contracts were not subsequently changed.

The current review fixes changed ordinary upgrade authoring plus card Presentation DTO/mapping/widget styling. Therefore current directly-invalidated gates are:

```text
1. Editor Build once
2. SlayTheSpireDemo.CardUpgrade once
3. SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper once
4. SlayTheSpireDemo.Phase6UIA2N.R4 once
```

The previous A3 ImmediatePreview PASS remains sticky because the gold-name pass did not modify A3 production source.

Do not expand to Phase6R / A2D5 / Shipping unless one of these focused gates exposes a shared-contract regression.

## Deferred / non-goals

Current ordinary-card Foundation still does not implement:

```text
RepeatableUpgradeCapability
Searing Blow
Armaments
Run Deck persistence
campfire / reward / shop
save/load
Phase 8
```

## Next exact action

```text
Run the current directly-invalidated gates.
If all PASS:
→ restore Card Expansion / Upgrade Foundation = COMPLETE / VALIDATED / SEALED
→ start Production Card Base/Plus Authoring
```
