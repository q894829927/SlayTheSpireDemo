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
DESIGN LOCKED

CFV-1 Card Metadata Contract:
COMPLETE / VALIDATED / SEALED

CFV-2 Card Face Shell:
IMPLEMENTATION AUTHORIZED / ASSET AUTHORING IN PROGRESS / NOT SEALED

CFV-3+:
NOT AUTHORIZED

Production Card Authoring:
UNBLOCKED / NOT STARTED / NO NEW IMPLEMENTATION AUTHORIZED
```

## Current authority

```text
docs/CardFaceVisualStyleImplementation.md
```

CFV-1 validation evidence:

```text
docs/CFV1Validation.md
```

Current implementation branch:

```text
cfv2-card-face-shell
```

The user explicitly authorized CFV-2 on 2026-09-05 after confirming CFV-1 had been fast-forwarded into `main`. CFV-2 authorization is limited to the locked Card Face Shell slice: canonical UMG shell construction plus geometry measurement/evidence. CFV-3 StyleSet/resolver work, CFV-4 production style authoring, CFV-5 visual acceptance, Card Expansion, and any later implementation remain separately gated.

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

## CFV-1 sealed contract

CFV-1 permanently establishes:

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

Validation evidence reported PASS:

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.CFV.CardMetadataContract
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged
WBP_BattleCard_Native Compile + Save
```

The focused CFV test directly exercises Native exact identity, so the conditional extra R8 identity gate was not required.

CFV-1 gates are sticky. Do not rerun them unless a later change directly invalidates a sealed contract.

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

This CFV plan only requires complete production authoring for `Red`. Green / Blue / Purple / Colorless / Curse configuration may remain absent until the corresponding confirmed future content enters production.

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

## CFV-2 active shell contract

CFV-2 may modify only the production card Widget shell and record geometry evidence. The current saved baseline is the 16-control `WBP_BattleCard_Native` documented in `docs/WBPSavedBlueprintSnapshot.md`.

Target shell:

```text
SB_Card
└─ Btn_Card
   └─ OV_Card
      └─ CN_CardFace
         ├─ Img_CardShadow
         ├─ Img_CardBackground
         ├─ Img_CardArt
         ├─ Img_CardFrame
         ├─ Img_CardBanner
         ├─ SB_CardName
         │  └─ Txt_CardName
         ├─ OV_CardType
         │  ├─ HB_CardTypePlate
         │  │  ├─ Img_TypeLeft
         │  │  ├─ Img_TypeCenter
         │  │  └─ Img_TypeRight
         │  └─ Txt_CardType
         ├─ Img_CostOrb
         ├─ Txt_Cost
         └─ Txt_CardDescription
```

CFV-2 rules:

```text
root remains 150 × 210
move existing core widgets where practical; do not recreate merely to rename
preserve core BindWidget names and identity
new decorative surfaces are presentation-only
no StyleSet implementation
no resolver implementation
no CardColor/Rarity runtime texture selection
no production texture authoring gate
no visual PIE acceptance
```

Geometry evidence to record before CFV-2 can seal:

```text
Attack PortraitRect
Skill PortraitRect
Power PortraitRect
Attack/Skill/Power TypePlateRect
used 512-cropped texture trim placements
final ZOrder / hit-test arrangement
```

The authority's initial positions are authoring starting points, not PASS evidence.

## CFV implementation slices

```text
CFV-1 — Card Metadata Contract        COMPLETE / VALIDATED / SEALED
CFV-2 — Card Face Shell               AUTHORIZED / IN PROGRESS
CFV-3 — StyleSet + Pure Resolver      NOT AUTHORIZED
CFV-4 — Production StyleSet / Assets  NOT AUTHORIZED
CFV-5 — Visual Acceptance             NOT AUTHORIZED
```

## Current stop point

```text
Card Upgrade STS-Style Refactor
→ COMPLETE / VALIDATED / SEALED

Card Face Visual Style
→ DESIGN LOCKED

CFV-1
→ COMPLETE / VALIDATED / SEALED

CFV-2
→ IMPLEMENTATION AUTHORIZED
→ ASSET AUTHORING IN PROGRESS
→ NOT VALIDATED
→ NOT SEALED

CFV-3+
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
AUTHOR WBP_BattleCard_Native CARD-FACE SHELL
→ Compile + Save
→ capture final shell / geometry evidence
→ STOP before CFV-3
```

Do not begin StyleSet implementation, resolver code, production texture authoring, visual PIE, or Production Card Expansion during CFV-2.
