# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Expansion / Upgrade Foundation:
SIMPLIFIED IMPLEMENTATION COMPLETE IN SOURCE / VALIDATION PENDING

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED
```

## Current active authority

```text
docs/CardUpgradeFoundationDesign.md
docs/CardUpgradeFoundationImplementation.md
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

## Focused validation source

Added:

```text
SlayTheSpireDemo.CardUpgrade.SingleVariant
SlayTheSpireDemo.CardUpgrade.EffectiveConsumers
```

The tests cover Base/Plus selection, one-time Action mutation, second-upgrade rejection, stable identity, authored Effect replacement, card text and committed card snapshot effective values.

Because `BattleManagerUIA3Preview` was migrated from `Definition->Effects` to `Card->GetEffects()`, the existing focused A3 immediate-preview gate is directly invalidated and should be rerun after the new CardUpgrade suite passes.

## Validation state

No local UE Build or Automation has been run from this tool environment.

Current status is therefore:

```text
Source implementation: COMPLETE
Static review: COMPLETE
Editor Build: PENDING USER RUN
CardUpgrade focused Automation: PENDING USER RUN
A3 directly-invalidated focused regression: PENDING USER RUN
Seal: PENDING
```

Do not claim PASS until those exact gates run.

## Deferred / non-goals

Current slice does not implement:

```text
RepeatableUpgradeCapability
Searing Blow
Armaments
Run Deck persistence
campfire / reward / shop
save/load
production card asset migration
Phase 8
```

Repeatable upgrading remains a later orthogonal capability and must not force ordinary cards away from the simple `bool + two authored configs` model.

## Next exact action

```text
1. User runs one Editor Build.
2. User runs SlayTheSpireDemo.CardUpgrade once.
3. If PASS, user runs the directly-invalidated A3 ImmediatePreview focused suite once.
4. Report results.
5. Record validation evidence and seal this simplified foundation if all pass.
6. Only then start production card Base/Plus authoring as the next bounded task.
```
