# CFV-3 StyleSet + Pure Resolver Execution

Date: 2026-09-05

Status:

```text
IMPLEMENTED ON BRANCH
TYPE-PLATE IMAGE PATH REMOVED BY USER DECISION
REVALIDATION PENDING AFTER REFLECTED BINDING CHANGE
NOT SEALED
```

Branch:

```text
cfv3-style-set-resolver
```

Base:

```text
cfv2-card-face-shell
edbf13e5a215846fa6ab65398c5bdbd0ffd7b940
```

## Current explicit execution decision

The user explicitly authorized starting CFV-3 before final Skill/Power visual tuning so the real resolver-driven result can be inspected first.

Current production observations supersede the earlier speculative geometry split where not needed:

```text
CardName position      = fixed Widget Designer geometry
OV_CardType position   = fixed Widget Designer geometry
Txt_CardType           = retained as the only card-type visual
Cost position          = fixed Widget Designer geometry
Description position   = fixed Widget Designer geometry
CardArt rect            = fixed for this first resolver pass
Img_CardShadow          = removed / not used in the current production shell
HB_CardTypePlate        = removed / not used
Img_TypeLeft            = removed / not used
Img_TypeCenter          = removed / not used
Img_TypeRight           = removed / not used
```

`OV_CardType` remains as a fixed Designer layout wrapper around `Txt_CardType`. CFV-3 does not move it at runtime. Rarity no longer owns type-plate image resources; rarity visual selection is limited to Frame + Banner.

The cropped atlas layers still carry placement because Attack/Skill/Power background/frame PNGs have different trim bounds. Those placements are authored in the current internal **300 x 420 texture-design space**; the WBP ScaleBox owns the presentation scaling to the external 150 x 210 card size.

## Implemented runtime contract

Added:

```text
FCardFaceLayerPlacement
FCardFaceTextureRegion
FCardColorVisualStyle
FCardRarityVisualStyle
FCardFaceStyleConfig
UCardFaceStyleSet
ECardFaceVisualShape
FResolvedCardFaceStyle
ResolveCardFaceStyle(...)
```

Resolver contract:

```text
CardType
→ Attack / Skill / Power visual shape
→ Status / Curse use Skill visual shape

CardColor
→ exact-key ColorStyles.Find only
→ Background + CostOrb
→ missing key does NOT fall back to Red

Rarity
→ Basic/Common/Special/Curse = CommonVisual
→ Uncommon = UncommonVisual
→ Rare = RareVisual

Rarity + visual shape
→ Frame

Rarity
→ Banner
```

No resolver path performs:

```text
Gameplay query
BattleManager query
LoadObject
texture pixel parsing
TMap iteration fallback
CardId branch
```

## Widget integration

`UBattleCardWidget` now has an explicit:

```text
EditDefaultsOnly UCardFaceStyleSet* CardFaceStyleSet
```

Optional decorative bindings:

```text
Img_CardBackground
Img_CardFrame
Img_CardBanner
Img_CostOrb
```

Core fail-closed bindings remain unchanged:

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

Refresh behavior:

```text
SetCardView
→ core text refresh
→ frozen CardArt refresh
→ pure style resolve
→ decorative Brush + 300x420 trim placement refresh
```

Null or missing visual configuration is presentation-only degradation:

```text
missing CardFaceStyleSet
→ core content remains
→ frozen CardArt remains
→ decorative surfaces clear + Hidden
→ input authority unchanged
```

Same-widget refresh explicitly clears old Brush and placement before missing styles can leave stale visuals.

## Focused Automation

```text
SlayTheSpireDemo.CFV.VisualResolver
SlayTheSpireDemo.CFV.WidgetStyle
```

Coverage includes:

```text
six CardColor exact-key selections
CardType → VisualShape mapping
Rarity visual collapse
missing ColorStyle has no Red fallback
same Widget style replacement
missing-style clear
restore after clear
300x420 texture placement replacement
null StyleSet core-content survival
missing optional decorative controls
upgrade state does not alter static CFV selection
```

## Validation state

The user previously reported Build + both CFV-3 Automation suites PASS before the type-plate image path was removed. Because the latest change removes reflected Widget bindings and StyleSet fields, those gates are invalidated once and must be rerun.

Required revalidation:

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.CFV.VisualResolver
SlayTheSpireDemo.CFV.WidgetStyle
Compile + Save + Reopen the production card Widget
```

No broad regression suite is required. After the production `UCardFaceStyleSet` is authored and assigned, perform one focused PIE visual check to judge the real Skill/Power result before deciding whether any geometry split is necessary.
