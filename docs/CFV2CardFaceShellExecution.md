# CFV-2 Card Face Shell — Execution Checklist

Date: **2026-09-05**

Status:

```text
IMPLEMENTATION AUTHORIZED
LOCAL UMG ASSET AUTHORING REQUIRED
VALIDATION PENDING
NOT SEALED
```

Authority:

```text
docs/CardFaceVisualStyleImplementation.md
```

This file is the CFV-2 execution/evidence worksheet. It does not replace the authority document.

---

## 1. Scope

CFV-2 is limited to:

```text
WBP_BattleCard_Native canonical shell
+ Attack / Skill / Power geometry authoring and measurement
+ Compile / Save evidence
```

CFV-2 does **not** implement:

```text
UCardFaceStyleSet
pure resolver
CardColor texture selection
Rarity texture selection
production StyleSet mapping
automated visual-style resolver tests
visual PIE acceptance
```

Those remain CFV-3+.

---

## 2. Current saved baseline

The current saved production card Widget is:

```text
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard_Native
```

Current Designer baseline:

```text
SB_Card : SizeBox (150 × 210)
└── Btn_Card : Button
    └── OV_Card : Overlay
        ├── BG_Card : Border
        ├── VB_CardContent : VerticalBox
        │   ├── SB_CardName : SizeBox (HeightOverride = 20)
        │   │   └── Txt_CardName
        │   ├── SB_CardArt : SizeBox (HeightOverride = 100)
        │   │   └── Img_CardArt
        │   └── Txt_CardType
        ├── SB_Description : SizeBox (120 × 60)
        │   └── Txt_CardDescription : RichTextBlock
        └── SB_Cost : SizeBox (34 × 34)
            └── OV_Cost : Overlay
                ├── Img_CostBase
                └── Txt_Cost
```

Core C++ binding names that must remain intact:

```text
Btn_Card
Txt_CardName
Txt_Cost
Txt_CardDescription
Txt_CardType
Img_CardArt
```

Do not delete/recreate these merely to restructure the hierarchy. Prefer reparenting/moving the existing Widget instances.

---

## 3. Target Designer hierarchy

Build toward:

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

The old `BG_Card`, `VB_CardContent`, `SB_CardArt`, `SB_Description`, `SB_Cost`, `OV_Cost`, and `Img_CostBase` may remain temporarily while the new shell is constructed, but they must not become a second layout authority. Collapse/remove obsolete containers only after all six core controls have been safely moved and the WBP compiles.

---

## 4. Canonical root and interaction contract

Lock:

```text
SB_Card WidthOverride  = 150
SB_Card HeightOverride = 210
```

Keep:

```text
Btn_Card = the sole card click entry
existing hover/selection/playback RenderTransform ownership unchanged
```

Decorative layers must not intercept input:

```text
Img_CardShadow
Img_CardBackground
Img_CardFrame
Img_CardBanner
Img_TypeLeft
Img_TypeCenter
Img_TypeRight
Img_CostOrb
→ HitTestInvisible
```

`OV_CardType` is a presentation positioning wrapper and must not become an input authority.

Do not enable clipping on a parent that would cut off the banner, cost orb, shadow, hover transform, or playback motion.

---

## 5. Fixed Designer geometry

Fixed public geometry remains Designer-owned:

```text
Root      = 150 × 210
Name Rect = initial (22, 7, 106, 25)
Cost text = keep centered over cost orb
Description Rect = initial (17, 133, 116, 57)
```

These are canonical starting values. Adjust only when the actual shell requires it; record final values below.

`SB_CardName` may remain as the title wrapper. Keep `Txt_CardName` single-line and centered.

`Txt_CardDescription` remains the existing `URichTextBlock` and must keep `DT_BattleCardTextStyles`.

---

## 6. Shape-dependent geometry measurement

CFV-2 does not yet implement `FCardFaceTypeLayout`. Instead, author/measure the final geometry in Designer and record the results for CFV-3 migration.

Starting references from the locked authority:

```text
Attack Frame ≈ Position (9, 31), Size (131, 92.5)
Skill Frame  ≈ Position (9, 30.5), Size (131.5, 91.5)
Power Frame  ≈ Position (7.5, 3), Size (134.5, 119)

Banner       ≈ Position (-6, 5.5), Size (162, 38.5)
Cost Orb     ≈ Position (-9.5, -8.5), Size (36, 35.5)

TypePlate center ≈ (75, 115), height 11.5
```

Do not treat these estimates as acceptance evidence. Final values must be read from the saved Designer result.

### Final evidence — fill after authoring

```text
Attack PortraitRect = PENDING
Skill PortraitRect  = PENDING
Power PortraitRect  = PENDING

Attack TypePlateRect = PENDING
Skill TypePlateRect  = PENDING
Power TypePlateRect  = PENDING

Name Rect        = PENDING
Description Rect = PENDING
Cost text Rect   = PENDING
```

Status / Curse do not get independent geometry in CFV-2; later they consume Skill VisualShape geometry.

---

## 7. Type plate construction

Use:

```text
OV_CardType
├─ HB_CardTypePlate
│  ├─ Img_TypeLeft   = Auto, target width ≈ 7, height 11.5
│  ├─ Img_TypeCenter = Fill, height 11.5
│  └─ Img_TypeRight  = Auto, target width ≈ 7.5, height 11.5
└─ Txt_CardType
```

`OV_CardType` is the future owner of `TypePlateRect`; bottom plate and text must move together.

Do not create separate Red/Green/Blue type-plate geometry.

---

## 8. Texture trim placement evidence

CFV-2 may use the existing cropped CardUI textures as visual guides for shell measurement, but it does not establish runtime selection logic.

For each used 512-cropped texture, record canonical placement using the locked atlas conversion rule:

```text
Source canvas = 512 × 512
Canonical body origin O = (106, 46)
Canonical body size   = (300, 420)
Uniform scale S       = 0.5

TrimTopLeft = (offset.x, orig.height - offset.y - size.height)
LocalPosition = (TrimTopLeft - O) * S
LocalSize     = size * S
```

Evidence table:

| Layer | Texture | Final Position | Final Size | Status |
|---|---|---:|---:|---|
| Shadow | `card_shadow` | PENDING | PENDING | PENDING |
| Red Attack Background guide | `bg_attack_red` | PENDING | PENDING | PENDING |
| Red Skill Background guide | `bg_skill_red` | PENDING | PENDING | PENDING |
| Red Power Background guide | `bg_power_red` | PENDING | PENDING | PENDING |
| Common Attack Frame guide | `frame_attack_common` | PENDING | PENDING | PENDING |
| Common Skill Frame guide | `frame_skill_common` | PENDING | PENDING | PENDING |
| Common Power Frame guide | `frame_power_common` | PENDING | PENDING | PENDING |
| Common Banner guide | `banner_common` | PENDING | PENDING | PENDING |
| Red Cost Orb guide | `card_red_orb` | PENDING | PENDING | PENDING |

Do not use `MatchSize` / native texture pixel size as card layout authority.

---

## 9. Required compile/save gate

After the shell has been authored:

```text
WBP_BattleCard_Native
→ Compile PASS
→ Save
→ Close/Reopen
→ confirm hierarchy and core bindings remain intact
```

No manual PIE is required for CFV-2. Final visual acceptance belongs to CFV-5.

No C++ Build/Automation is required solely for Designer-only CFV-2 changes unless an actual C++ change is introduced, which is outside the planned CFV-2 scope.

---

## 10. CFV-2 completion evidence

Do not mark CFV-2 complete until all are true:

```text
[ ] canonical 150 × 210 shell saved
[ ] all six core binding names preserved
[ ] all planned decorative controls created
[ ] OV_CardType + three-piece type plate created
[ ] decorative hit-test contract correct
[ ] no unintended parent clipping
[ ] final Attack / Skill / Power PortraitRect recorded
[ ] final Attack / Skill / Power TypePlateRect recorded
[ ] used texture trim placements recorded
[ ] WBP Compile PASS
[ ] WBP Save PASS
[ ] close/reopen structure check PASS
```

Then record the final values here and in the CFV authority/evidence update, seal CFV-2, and STOP before CFV-3.
