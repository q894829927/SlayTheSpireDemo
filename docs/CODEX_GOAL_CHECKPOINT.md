# Codex Goal Checkpoint — Card Face Visual Style

Last updated: **2026-09-05**

## Current status

```text
Phase 6UI-A / A3:
COMPLETE / VALIDATED / SEALED

Phase 7A–7F:
COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
COMPLETE / VALIDATED / SEALED

Card Face Visual Style (CFV):
DESIGN LOCKED / IMPLEMENTATION NOT AUTHORIZED

CFV-1 Card Metadata Contract:
NOT STARTED / NOT AUTHORIZED

Production Card Authoring:
UNBLOCKED / NOT STARTED / NO NEW IMPLEMENTATION AUTHORIZED
```

## Current authority

```text
docs/CardFaceVisualStyleImplementation.md
```

The CFV authority has completed design review and is now locked. Do not begin CFV-1, Card Expansion, or any later CFV slice without a new explicit user authorization.

The sealed Card Upgrade authority remains:

```text
docs/CardUpgradeSTSStyleRefactor.md
```

It is predecessor/background authority only and must not be reopened for CFV unless a concrete future requirement directly invalidates one of its locked contracts.

## Locked CFV model

Semantic / frozen axes:

```text
CardType
CardRarity
CardColor
Upgrade State
```

Core rules:

```text
CardColor != character identity
CardType / CardRarity / CardColor / bUpgraded remain orthogonal

ECardColor:
Red / Green / Blue / Purple / Colorless / Curse

migration defaults:
Rarity    = Common
CardColor = Red
```

The defaults exist for backward-compatible serialization only. Production CardData must explicitly author its real metadata when it enters the CFV production-authoring slice.

Status / Curse standard production convention:

```text
Status
→ CardType  = Status
→ CardColor = Colorless

Curse
→ CardType  = Curse
→ CardColor = Curse
→ Rarity    = Curse
```

These remain explicit content-authoring values; the data model does not implicitly couple `ECardType::Curse` to `ECardColor::Curse`.

## Frozen Presentation contract

Planned CFV-1 propagation is:

```text
UCardData.Rarity / CardColor
        ↓
UCardInstance getters
        │
        ├─ formal/current Hand freeze
        │      ↓
        │ FBattleHUDCardView
        │
        └─ committed/historical snapshot
               ↓
        FPresentationCardSnapshot
               ↓
        PresentationCardView
               ↓
        FBattleHUDCardView
```

`FCardReadView` is not expanded with duplicate Rarity/CardColor fields.

Generic exact card-face continuity will include:

```text
bUpgraded
Rarity
CardColor
```

`RichDescription` remains intentionally excluded from generic Hand identity comparison while still being propagated by the historical mapper.

## Locked visual architecture

```text
CardType
→ ECardFaceVisualShape
   Attack / Skill / Power
   Status / Curse → Skill visual shape

CardColor + VisualShape
→ Background

CardColor
→ CostOrb

CardRarity
→ Common / Uncommon / Rare visual rarity

VisualRarity + VisualShape
→ Frame

VisualRarity
→ Banner / TypePlate

bUpgraded
→ "+" / #7FFF00
```

`FallbackFrame` is not a normal Status/Curse production path.

## Style configuration boundary

`UCardFaceStyleSet` is a narrow authored Presentation configuration asset, not a Registry, Service, singleton, Gameplay definition, runtime discovery system, or universal skin framework.

Ownership:

```text
WBP_BattleCard_Native
→ EditDefaultsOnly CardFaceStyleSet reference

UCardFaceStyleSet
→ ColorStyles
→ shared RarityStyles
→ shared Attack/Skill/Power TypeLayouts
→ trimmed TextureRegion placements

pure resolver
→ consumes frozen DTO metadata + StyleSet.Config
```

Forbidden:

```text
CardData / CardInstance → StyleSet reference
CardColor → hard-coded LoadObject path
Global Visual Registry
TMap iteration fallback
missing Green/Blue/etc. → Red fallback
```

This slice only requires complete production authoring for `Red`. Green / Blue / Purple / Colorless / Curse configuration may remain absent until the corresponding confirmed future content enters production.

## Widget / degradation contract

Core fail-closed surface remains:

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

Decorative / optional presentation surface:

```text
Img_CardShadow
Img_CardBackground
Img_CardFrame
Img_CardBanner
Img_TypeLeft
Img_TypeCenter
Img_TypeRight
Img_CostOrb
OV_CardType
```

Missing decorative controls, missing textures, an unconfigured valid ColorStyle, or `CardFaceStyleSet == nullptr` may degrade/hide visuals but must not disable Gameplay/input.

Null StyleSet specifically keeps core DTO refresh, frozen CardArt and card request behavior functional while clearing/hiding decorative CFV state.

## Geometry ownership

```text
WBP Designer
→ fixed 150 × 210 canonical card geometry
→ fixed Name / Cost / Description / interaction geometry

FCardFaceTypeLayout
→ PortraitRect
→ TypePlateRect

FCardFaceTextureRegion.Placement
→ sole trimmed texture placement authority
→ sole Frame placement authority
```

Color does not own a separate layout. `OV_CardType` receives `TypePlateRect` so the type plate and type text move as one presentation unit.

## CFV implementation slices

```text
CFV-1 — Card Metadata Contract
CFV-2 — Card Face Shell
CFV-3 — StyleSet + Pure Resolver
CFV-4 — Production StyleSet / Asset Authoring
CFV-5 — Visual Acceptance
```

Current next slice, once explicitly authorized:

```text
CFV-1 — Card Metadata Contract
```

Its scope is limited to Rarity + CardColor semantic metadata, authoring validation, current/frozen/historical propagation, exact continuity updates, and focused Automation/Blueprint compile-save evidence. It does not include StyleSet implementation, card-face shell layout, texture authoring, or PIE.

## Planned CFV-1 gates

```text
1. SlayTheSpireDemoEditor Win64 Development Build once
2. SlayTheSpireDemo.CFV.CardMetadataContract once
3. SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged once
```

If the new CFV test does not directly exercise the Native exact-identity comparison path, add only:

```text
SlayTheSpireDemo.Phase6UIA2N.R8.CardPlayed.ExactIdentityFinishAndCancel
```

Do not automatically run full R8, Phase6R, A2D5, Shipping, broad scenario replay, or manual PIE for CFV-1.

Blueprint asset gate after the reflected DTO change:

```text
Compile + Save WBP_BattleCard_Native
```

Only process another dependent Blueprint if UE explicitly reports it affected.

## Current stop point

```text
Card Upgrade STS-Style Refactor
→ COMPLETE / VALIDATED / SEALED

Card Face Visual Style
→ DESIGN LOCKED
→ IMPLEMENTATION NOT AUTHORIZED

CFV-1
→ NOT STARTED
→ NOT AUTHORIZED

Phase 8
→ remains deferred

Production Card Expansion
→ technically unblocked
→ NOT STARTED
→ no new card implementation authorized
```

Next action:

```text
WAIT FOR EXPLICIT USER AUTHORIZATION TO START CFV-1
```

No Build, Automation, Blueprint mutation, asset authoring, or PIE belongs to this checkpoint/status-sync step.
