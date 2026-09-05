# Codex Goal Checkpoint — Card Face Visual Style

Last updated: **2026-09-06**

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
DESIGN LOCKED / IMPLEMENTATION THROUGH CFV-4 COMPLETE

CFV-1 — Card Metadata Contract:
COMPLETE / VALIDATED / SEALED

CFV-2 — Card Face Shell:
PREDECESSOR IMPLEMENTED; SEE EXECUTION RECORD

CFV-3 — StyleSet + Pure Resolver:
COMPLETE / VALIDATED / SEALED

CFV-4 — Production StyleSet / Asset Authoring:
COMPLETE / USER-VALIDATED / SEALED

CFV-5 — Visual Acceptance:
NOT AUTHORIZED
```

## Current authority and execution records

Design authority:

```text
docs/CardFaceVisualStyleImplementation.md
```

Execution / validation records:

```text
docs/CFV1Validation.md
docs/CFV2CardFaceShellExecution.md
docs/CFV3StyleSetResolverExecution.md
docs/CFV4ProductionStyleSetExecution.md
```

Current implementation branch:

```text
cfv3-style-set-resolver
```

This checkpoint supersedes the earlier CFV-2-era authorization snapshot. Historical slice-specific evidence remains in the execution records above.

## Locked semantic model

```text
CardType
CardRarity
CardColor
Upgrade State
```

The axes remain orthogonal.

```text
ECardColor:
Red / Green / Blue / Purple / Colorless / Curse

migration defaults:
Rarity    = Common
CardColor = Red
```

Migration defaults are serialization fallbacks only. Production content owns its explicit semantic metadata.

## Locked visual resolver model

```text
CardType
→ ECardFaceVisualShape
→ Attack / Skill / Power
→ Status / Curse use Skill visual shape

CardColor + VisualShape
→ Background

CardColor
→ CostOrb

CardRarity
→ CommonVisual / UncommonVisual / RareVisual

VisualRarity + VisualShape
→ Frame

VisualRarity
→ Banner

bUpgraded
→ display-name "+" / upgraded title color
```

The current sealed production shell does **not** use:

```text
TypeLeft
TypeCenter
TypeRight
CardShadow
```

Rarity no longer owns a TypePlate image path. `Txt_CardType` remains the card-type visual inside fixed Designer geometry.

## CFV-3 sealed runtime boundary

`UCardFaceStyleSet` remains a narrow authored Presentation configuration asset.

The pure resolver consumes frozen card-face metadata plus the StyleSet and does not perform:

```text
Gameplay query
BattleManager query
LoadObject
texture pixel parsing
TMap iteration fallback
CardId branch
missing ColorStyle → Red fallback
```

`UBattleCardWidget` consumes the resolved presentation only. Missing decorative configuration may degrade/hide decorative surfaces but must not disable core DTO content or input behavior.

CFV-3 focused validation is sealed:

```text
SlayTheSpireDemoEditor Win64 Development Build      PASS
SlayTheSpireDemo.CFV.VisualResolver                 PASS
SlayTheSpireDemo.CFV.WidgetStyle                    PASS
Production card Widget modification / compile-save PASS
```

Do not repeat those gates unless a later change modifies the resolver contract, reflected StyleSet layout, or reflected Widget bindings.

## CFV-4 production StyleSet state

Production asset:

```text
DA_CardFaceStyleSet
```

Authored configuration:

```text
ColorStyles[Red]
├─ AttackBackground
├─ SkillBackground
├─ PowerBackground
└─ CostOrb

CommonStyle
├─ Banner
├─ AttackFrame
├─ SkillFrame
└─ PowerFrame

UncommonStyle
├─ Banner
├─ AttackFrame
├─ SkillFrame
└─ PowerFrame
```

Production Widget assignment:

```text
Class Defaults
→ Battle HUD | Card | Style
→ Card Face Style Set
→ DA_CardFaceStyleSet
```

User-reported focused PIE inspection is complete for the currently authored production set.

Representative coverage:

```text
Strike / Pommel Strike / Twin Strike → Attack path
Defend                              → Skill path
Inflame                             → Power path
Uppercut / Inflame                  → Uncommon visual path
```

Configured but not independently covered:

```text
CommonStyle.PowerFrame      CONFIGURED / NOT COVERED
UncommonStyle.SkillFrame    CONFIGURED / NOT COVERED
```

These two coverage gaps do not reopen CFV-4 authoring and must not be relabeled as independently validated without an actual matching card or focused fixture.

## Current stop point

```text
CFV-3
→ COMPLETE / VALIDATED / SEALED

CFV-4
→ COMPLETE / USER-VALIDATED / SEALED

CFV-5
→ NOT AUTHORIZED

Production Card Expansion
→ no new authorization implied by this checkpoint
```

Next gated work:

```text
CFV-5 — Visual Acceptance
```

Do not begin CFV-5, reopen sealed Gameplay/Presentation/Upgrade ownership, or modify the sealed CFV-3 resolver contract unless the user explicitly authorizes that work or a concrete acceptance finding requires a targeted correction.
