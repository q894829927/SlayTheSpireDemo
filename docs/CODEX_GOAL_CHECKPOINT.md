# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-05**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
COMPLETE / VALIDATED / SEALED

Previous FCardUpgradeConfig Foundation:
HISTORICAL / SUPERSEDED / SOURCE FIELDS REMOVED

Production Card Authoring:
UNBLOCKED / NOT STARTED / NO NEW IMPLEMENTATION AUTHORIZED
```

## Current authority

```text
docs/CardUpgradeSTSStyleRefactor.md
```

The Upgrade authority is now sealed. Do not reopen it for ordinary card expansion unless a future requirement directly invalidates one of its locked contracts.

## Sealed upgrade model

```text
UCardData
→ one Description
→ BaseCost / UpgradedCost
→ one Effects[]
→ typed Base / Upgraded values per Effect

UCardInstance
→ Definition
→ RuntimeId
→ bool bUpgraded
```

Runtime authority:

```text
CanUpgrade()          -> valid Definition && !bUpgraded
GetDescriptionFormat()-> Definition->Description
GetCurrentCost()      -> bUpgraded ? UpgradedCost : BaseCost
ResolveDestination()  -> Definition->DefaultDestination
GetEffects()          -> always Definition->Effects
```

In-combat mutation remains:

```text
UUpgradeCardAction
→ UCardInstance::CommitUpgrade()
→ bUpgraded false -> true once
```

Creation-time upgraded state remains supported via:

```cpp
UCardInstance::Initialize(Definition, RuntimeId, bStartUpgraded)
```

## Typed Effect upgrade surface

```text
Damage:
BaseAmount / UpgradedAmount
HitCount / UpgradedHitCount

Block:
BaseAmount / UpgradedAmount

Draw:
DrawCount / UpgradedDrawCount

ApplyStatus:
Amount / UpgradedAmount
```

Effective consumers use bool-only helpers and share the same upgraded value source across Gameplay / Dynamic Text / A3.

## Presentation contract

```text
Gameplay/DTO DisplayName
→ always authored base name

UBattleCardWidget
→ bUpgraded == false: DisplayName + default title style
→ bUpgraded == true:  DisplayName + "+" + upgraded title color
```

Upgraded title color:

```text
sRGB #7FFF00
```

No second authored upgraded DisplayName exists.

## Legacy cleanup complete

Removed from active source/model:

```text
FCardUpgradeConfig
bHasUpgrade
UCardData::Upgrade
UCardVariantData compatibility shim
```

Six production assets migrated and validated:

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

After final `UCardVariantData` removal:

```text
SlayTheSpireDemoEditor Win64 Development Build: PASS
full Editor restart: DONE
all six production DA_Card_* assets open normally: PASS
```

## Validation evidence

Sticky PASS evidence for the sealed Upgrade surface:

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.CardUpgrade
SlayTheSpireDemo.Phase6UIA3.DynamicText
SlayTheSpireDemo.UIA3.ImmediatePreview
SlayTheSpireDemo.Phase6C
SlayTheSpireDemo.Phase6UIA2N.R4
focused PIE
```

Focused PIE confirmed:

```text
normal card
→ authored name / default title color

same runtime card upgraded
→ authored name + "+"
→ #7FFF00 upgraded title
→ upgraded numeric text
→ actual Gameplay uses the same upgraded values
```

## Current stop point

```text
Card Upgrade STS-Style Refactor
→ COMPLETE / VALIDATED / SEALED

Phase 8
→ remains deferred

Production Card Expansion
→ technically unblocked
→ NOT STARTED
→ no Batch / CAP implementation is currently authorized
```

Do not start Bash / Iron Wave / Shrug It Off / Clothesline or CAP-02 merely because they were discussed as possible next directions. A new explicit user request is required before starting another implementation slice.

## Preserved non-goals / future independent capabilities

```text
repeatable upgrade / UpgradeCount / Searing Blow
UpgradedDescriptionOverride
effect count/type structural replacement
universal Upgrade Delta / Upgrade Context
second authored upgraded DisplayName
Armaments content implementation
Phase 8 implementation
save/load/run-deck persistence
campfire/reward/shop upgrade UX
```

These are not pending defects in the sealed ordinary-card upgrade foundation.
