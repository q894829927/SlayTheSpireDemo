# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Expansion / Upgrade Foundation:
COMPLETE / VALIDATED / SEALED

Production Card Base/Plus Authoring:
NEXT BOUNDED TASK / NOT STARTED

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED
```

## Sealed Upgrade Foundation authority

```text
docs/CardUpgradeFoundationDesign.md
docs/CardUpgradeFoundationImplementation.md
docs/CardUpgradeFoundationValidation.md
```

The user explicitly simplified ordinary card upgrades on 2026-09-04:

```text
one UCardData identity
├─ existing Base configuration
└─ optional full UpgradedVariant configuration

UCardInstance
└─ bool bUpgraded
```

This supersedes older CAP-01 / Upgrade draft wording that proposed a generic typed upgrade-delta/effective-facts framework for ordinary cards.

Ordinary cards do **not** use a global `UpgradeLevel`, parameter patch interpreter, expression language, or Damage/Block/Draw-specific upgrade logic.

## Implemented source shape

```text
UCardData
├─ CardId                       // shared identity
├─ existing Base fields
│  ├─ DisplayName / Description / CardArt
│  ├─ CardType / TargetType / BaseCost / DefaultDestination
│  └─ Effects[]
└─ optional UCardVariantData UpgradedVariant
   ├─ DisplayName / Description / CardArt
   ├─ CardType / TargetType / Cost / DefaultDestination
   └─ Effects[]

UCardInstance
├─ Definition
├─ RuntimeId
└─ bUpgraded
```

Effective access is centralized on `UCardInstance`:

```text
GetDisplayName
GetDescriptionFormat
GetCardArt
GetCardType
GetCurrentCost
GetTargetType
ResolveDestination
GetEffects
```

Consumers migrated to this boundary:

```text
PlayCardAction
BattleTextResolver
BattleManagerUIA3Preview
PresentationCardSnapshotBuilder
BattleManager current-state Presentation freeze
```

Base and Plus remain the same `CardId / UCardInstance / RuntimeId`.

## Mutation authority

```text
UUpgradeCardAction
→ CardInstance::CommitUpgrade
→ false -> true once
```

Second upgrade is a generic fail-soft rejection and must not create a ResolutionFault.

No `CardUpgradedEvent` is introduced in this slice.

## Validation evidence

Added focused tests:

```text
SlayTheSpireDemo.CardUpgrade.SingleVariant
SlayTheSpireDemo.CardUpgrade.EffectiveConsumers
```

The implementation directly changed A3 preview effect selection from authored Base `Definition->Effects` to effective `Card->GetEffects()`, so the existing A3 ImmediatePreview focused gate was the only directly invalidated sealed regression gate.

On 2026-09-04 the user reported the prescribed local UE5.8 gates all passing:

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
SlayTheSpireDemo.CardUpgrade: PASS
SlayTheSpireDemo.UIA3.ImmediatePreview: PASS
```

No exact test counts were supplied, so none are inferred here.

This seals the simplified ordinary-card Upgrade Foundation. Detailed evidence: `docs/CardUpgradeFoundationValidation.md`.

## Sticky validation rule

The passing Foundation gates are sticky.

Do not rerun Build/CardUpgrade/A3 merely because production card DataAssets are authored next. Rerun only when a shared Foundation contract is changed or a concrete regression invalidates the evidence.

## Deferred / non-goals

The sealed Foundation does not implement:

```text
RepeatableUpgradeCapability
Searing Blow
Armaments
Run Deck persistence
campfire / reward / shop
save/load
Phase 8
```

Repeatable upgrading remains a later orthogonal capability and must not force ordinary cards away from the simple `bool + two authored configs` model.

## Next exact action

```text
Production Card Base/Plus Authoring
```

Start with a small first batch of real cards using the sealed Foundation. Do not reopen or redesign Upgrade Foundation unless real production-card requirements prove the contract insufficient.
