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
BLOCKED ONLY BY THE TWO DIRECTLY-INVALIDATED REVALIDATION GATES

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED
```

## Current Upgrade Foundation authority

```text
docs/CardUpgradeFoundationDesign.md
docs/CardUpgradeFoundationImplementation.md
docs/CardUpgradeFoundationValidation.md
```

The user further simplified ordinary-card upgrade authoring after the first validated version.

Current authoritative model:

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

`UCardVariantData` / `UpgradedVariant` is no longer part of ordinary-card authoring.

## Stable metadata rule

These values are authored once and are not duplicated in Upgrade:

```text
DisplayName
CardArt
CardType
TargetType
```

Visible upgraded name is derived:

```text
Strike → Strike+
```

The `+` is not a second authored DisplayName.

## Effective boundary

```text
stable:
GetDisplayName()   // derives + when upgraded
GetCardArt()
GetCardType()
GetTargetType()

Base/Upgrade-sensitive:
GetDescriptionFormat()
GetCurrentCost()
ResolveDestination()
GetEffects()
```

Gameplay / Preview / Presentation do not directly branch on `bUpgraded`.

## Mutation authority

```text
UUpgradeCardAction
→ CardInstance::CommitUpgrade
→ false -> true once
```

Second upgrade remains generic fail-soft and does not ResolutionFault.

## Historical validation and current invalidation

Before this review fix, the user reported:

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

Those results remain historical evidence.

The subsequent slim-authoring review fix changed `CardData`, `CardInstance`, BattleText definition validation and CardUpgrade focused tests. It did **not** modify A3 production source again.

Therefore current revalidation is only:

```text
1. Editor Build once
2. SlayTheSpireDemo.CardUpgrade once
```

The previous A3 ImmediatePreview PASS remains sticky.

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
Run the two directly-invalidated gates.
If both PASS:
→ restore Card Expansion / Upgrade Foundation = COMPLETE / VALIDATED / SEALED
→ start Production Card Base/Plus Authoring
```
