# CFV-1 Card Metadata Contract — Validation Evidence

Date: **2026-09-05**

Status:

```text
COMPLETE / VALIDATED / SEALED
```

Authority:

```text
docs/CardFaceVisualStyleImplementation.md
```

Implementation branch:

```text
cfv1-card-metadata-contract
```

## Scope sealed by this evidence

CFV-1 establishes only the locked Card Metadata Contract:

```text
ECardRarity
ECardColor
UCardData authored metadata + editor enum validation
UCardInstance definition-backed metadata getters
formal/current Hand frozen propagation
historical FPresentationCardSnapshot propagation
PresentationCardView mapping
Native + diagnostic frozen enum validation
Native + diagnostic exact continuity for bUpgraded / Rarity / CardColor
RichDescription intentionally excluded from generic Hand identity comparison
```

`FCardReadView` remains unchanged. No StyleSet, card-face shell, texture authoring, resolver, or visual PIE work is part of CFV-1.

## Validation evidence

The user reported all locked CFV-1 gates passing locally on 2026-09-05:

```text
SlayTheSpireDemoEditor Win64 Development Build
→ PASS

SlayTheSpireDemo.CFV.CardMetadataContract
→ PASS

SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged
→ PASS

WBP_BattleCard_Native Compile + Save
→ PASS
```

The focused `SlayTheSpireDemo.CFV.CardMetadataContract` directly exercises the Native exact-identity contract, including:

```text
matching bUpgraded / Rarity / CardColor accepted
bUpgraded mismatch rejected
Rarity mismatch rejected
CardColor mismatch rejected
invalid frozen Rarity rejected
invalid frozen CardColor rejected
RichDescription mismatch intentionally accepted by generic identity comparison
```

Therefore the conditional extra R8 exact-identity test was not required by the locked validation plan.

## Validation budget result

```text
Build once
focused CFV Automation once
focused historical CardZoneChanged regression once
WBP_BattleCard_Native Compile + Save
no manual PIE
no broad regression suite
```

No additional validation is required for CFV-1 unless a later change directly invalidates one of these gates.

## Seal

CFV-1 is now:

```text
COMPLETE
VALIDATED
SEALED
```

Sticky gate rule applies: do not rerun CFV-1 validation merely because a later CFV slice starts. Rerun only a gate directly invalidated by a later change.

CFV-2 and later slices remain separately authorized boundaries and are **NOT AUTHORIZED** by this seal.
