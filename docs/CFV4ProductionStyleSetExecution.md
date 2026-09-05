# CFV-4 Production StyleSet / Asset Authoring Execution

Date: 2026-09-06

Status:

```text
COMPLETE
USER-VALIDATED
SEALED
```

Branch:

```text
cfv3-style-set-resolver
```

Predecessor:

```text
CFV-3 — StyleSet + Pure Resolver
COMPLETE / VALIDATED / SEALED
```

## Scope completed

The production `DA_CardFaceStyleSet` has been authored, assigned to the actual production card Widget, and inspected in PIE.

The final CFV-3 reflected layout is used. The production StyleSet no longer contains or depends on:

```text
TypeLeft
TypeCenter
TypeRight
CardShadow
```

The authored production configuration is limited to the currently required Red + CommonVisual + UncommonVisual surfaces.

## Production StyleSet authoring

Configured:

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

The StyleSet remains presentation-only. No CardData/CardInstance ownership was added and no resolver fallback behavior was changed.

## Production Widget assignment

The production card Widget Class Default is assigned to:

```text
Battle HUD | Card | Style
→ Card Face Style Set
→ DA_CardFaceStyleSet
```

This is the real runtime Widget assignment, not a preview/test-only reference.

## PIE coverage

The current representative cards provide the following useful visual coverage:

```text
Strike / Pommel Strike / Twin Strike
→ Attack visual path

Defend
→ Skill visual path

Inflame
→ Power visual path

Uppercut / Inflame
→ Uncommon visual rarity path
```

Observed production coverage therefore exercises:

```text
Red AttackBackground
Red SkillBackground
Red PowerBackground
Red CostOrb
CommonVisual Banner
CommonVisual AttackFrame
CommonVisual SkillFrame
UncommonVisual Banner
UncommonVisual AttackFrame
UncommonVisual PowerFrame
```

The semantic Basic/Common/Special/Curse rarity collapse remains the resolver contract from CFV-3; this document refers to `CommonStyle` as the visible CommonVisual style, not as a claim that every card using it has semantic rarity `Common`.

## Configured but not independently covered

The following resources are authored but are not independently exercised by the current representative card set:

```text
CommonStyle.PowerFrame
CONFIGURED / NOT COVERED

UncommonStyle.SkillFrame
CONFIGURED / NOT COVERED
```

They must not be relabeled as independently validated until a production card or focused acceptance fixture exercises those exact combinations.

## Validation boundary

User-reported completion for CFV-4 includes:

```text
DA_CardFaceStyleSet authored               COMPLETE
Red ColorStyle authored                    COMPLETE
Common Style authored                      COMPLETE
Uncommon Style authored                    COMPLETE
Production Widget StyleSet assignment      COMPLETE
Focused PIE visual inspection              COMPLETE
```

No CFV-3 Build or Automation rerun is required for this CFV-4 authoring-only completion because the sealed resolver contract and reflected Widget bindings were not changed.

If a later change modifies the resolver, reflected StyleSet layout, or reflected Widget bindings, the relevant sealed CFV-3 gates become invalid and must be reconsidered.

## Seal decision

CFV-4 is complete for the currently authorized production authoring scope:

```text
CFV-4 — Production StyleSet / Asset Authoring
COMPLETE / USER-VALIDATED / SEALED
```

Known coverage gaps are explicitly recorded above and do not reopen CFV-4 authoring.

## Next gated work

```text
CFV-5 — Visual Acceptance
NOT AUTHORIZED BY THIS RECORD
```

CFV-5 should evaluate the actual production card-face result as a visual acceptance pass. Do not reopen Gameplay, BattleAction, Presentation Timeline, A3 Preview, Card Upgrade ownership, or the sealed CFV-3 resolver contract unless a concrete acceptance finding requires it.
