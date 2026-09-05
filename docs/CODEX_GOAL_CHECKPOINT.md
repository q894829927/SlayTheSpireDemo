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
DESIGN LOCKED / CFV-1 IMPLEMENTATION AUTHORIZED

CFV-1 Card Metadata Contract:
SOURCE IMPLEMENTED / VALIDATION PENDING / NOT SEALED

CFV-2+:
NOT AUTHORIZED

Production Card Authoring:
UNBLOCKED / NOT STARTED / NO NEW IMPLEMENTATION AUTHORIZED
```

## Current authority

```text
docs/CardFaceVisualStyleImplementation.md
```

The user explicitly authorized CFV-1 on 2026-09-05. That authorization is limited to the locked CFV-1 Card Metadata Contract slice. CFV-2, Card Expansion, and later CFV slices still require separate explicit authorization.

Implementation branch:

```text
cfv1-card-metadata-contract
```

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

## CFV-1 implemented source contract

The implementation branch now carries:

```text
ECardRarity
ECardColor
UCardData.Rarity / CardColor
UCardData::IsDataValid enum-domain validation
UCardInstance::GetRarity / GetCardColor
FPresentationCardSnapshot.Rarity / CardColor
FBattleHUDCardView.Rarity / CardColor
formal/current Hand freeze propagation
historical snapshot propagation
PresentationCardView mapper propagation
Native frozen-payload validation
Diagnostic frozen-payload validation
Native exact continuity: bUpgraded + Rarity + CardColor
Diagnostic exact continuity: bUpgraded + Rarity + CardColor
RichDescription intentionally excluded from generic continuity comparison
```

`FCardReadView` remains unchanged; Rarity/CardColor are read through `Source.Card` / `UCardInstance` at the formal Hand freeze boundary.

The focused CFV Automation test also directly exercises the Native exact-identity and Native frozen-enum validation paths. Therefore the conditional extra R8 identity gate is not required unless the focused CFV test itself fails in a way that specifically points to that historical path.

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

Current active slice:

```text
CFV-1 — Card Metadata Contract
```

Its scope is limited to Rarity + CardColor semantic metadata, authoring validation, current/frozen/historical propagation, exact continuity updates, and focused Automation/Blueprint compile-save evidence. It does not include StyleSet implementation, card-face shell layout, texture authoring, or PIE.

## CFV-1 remaining gates

Run in this order and only once unless a gate fails and is directly invalidated by the fix:

```text
1. SlayTheSpireDemoEditor Win64 Development Build once
2. SlayTheSpireDemo.CFV.CardMetadataContract once
3. SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged once
```

The focused CFV test now directly exercises Native exact identity, so do not add the conditional R8 identity test by default.

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
→ CFV-1 IMPLEMENTATION AUTHORIZED

CFV-1
→ SOURCE IMPLEMENTED
→ VALIDATION PENDING
→ NOT SEALED

CFV-2+
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
RUN CFV-1 BUILD + FOCUSED AUTOMATION + WBP COMPILE/SAVE GATES
```

Do not begin CFV-2 until CFV-1 is validated/sealed and the user explicitly authorizes the next slice.
