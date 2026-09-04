# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
DESIGN APPROVED / AUTHORITY LOCKED / IMPLEMENTATION NOT STARTED

Previous FCardUpgradeConfig Foundation:
HISTORICAL / SUPERSEDED / DO NOT REVALIDATE AS FINAL TARGET

Production Card Authoring:
BLOCKED BY STS-STYLE REFACTOR + SIX-ASSET MIGRATION

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED
```

## Current authority

```text
docs/CardUpgradeSTSStyleRefactor.md
```

Historical only:

```text
docs/CardUpgradeFoundationDesign.md
docs/CardUpgradeFoundationImplementation.md
docs/CardUpgradeFoundationValidation.md
```

## Locked target model

```text
UCardData / CardEffects = immutable shared definitions
UCardInstance::bUpgraded = single ordinary-card mutable truth

UCardData
├─ shared identity/presentation/rules
├─ Description
├─ BaseCost
├─ UpgradedCost
├─ DefaultDestination
└─ one Effects[]

Effects
→ typed Base / Upgraded authored fields only where that Effect owns the value
```

Remove as ordinary-card authority:

```text
bHasUpgrade
FCardUpgradeConfig
second Upgrade.Description
second Upgrade.Destination
second Upgrade.Effects[]
```

## Effective-value boundary

Effect resolver API is deliberately narrow:

```cpp
GetEffectiveXXX(bool bIsUpgraded)
```

not:

```cpp
GetEffectiveXXX(UCardInstance*)
```

Calling boundary freezes `Card->IsUpgraded()` and passes only the bool.

Current build-time freeze is legal because the card being played does not change its own `bUpgraded` during that resolution. If a future mechanic can change that state mid-resolution, affected effective-value resolution must move to Action Execute-time.

No `Upgraded*=0` fallback semantics are permitted. Every Upgraded field is explicitly authored and independently validated.

## Presentation state remains locked

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ upgraded name gold
```

DisplayName text itself remains unchanged. No `+` suffix in Gameplay.

## Migration scope

All six current production card assets are in scope:

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

Migration rule:

```text
old model still present
→ add new typed Upgraded* fields
→ parity-check/copy old Upgrade values into new fields
→ only after parity remove old serialized ordinary-upgrade fields
→ USER ACTION REQUIRED: open/resave all six assets in UE Editor
```

`UCardVariantData` load shim stays until asset load/resave evidence proves removal is safe.

## Test migration

Do not add a parallel new upgrade test file.

Migrate:

```text
Source/SlayTheSpireDemoTests/Private/CardUpgradeFoundationTests.cpp
```

Remove old two-object assertions and prove:

```text
same CardId / RuntimeId
same Effects object identity across upgrade
bUpgraded false -> true once
base values before upgrade
upgraded typed values after upgrade
same DisplayName text
frozen bUpgraded propagation
Gameplay / Dynamic Text / A3 value parity
```

## Final validation budget

Final-head gates for the refactor are:

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade
3. SlayTheSpireDemo.UIA3.DynamicText
4. SlayTheSpireDemo.UIA3.ImmediatePreview
5. SlayTheSpireDemo.Phase6C
6. one focused PIE visual pass
```

Phase6C is required because current Effect `BuildActions` authored-value reads are modified.

Do not run full Phase 6 / Phase 7.

The previously pending PresentationCardViewMapper/R4 gates belonged to the superseded intermediate gold-name implementation state; they do not need to be run merely to seal that obsolete model. Their relevant upgraded-state mapping behavior remains part of the final refactor code and can be investigated only if focused final gates expose a Presentation regression.

## Phase 8 relation

Phase 8 remains deferred and non-blocking.

The new spawn/starting-card upgraded-state spec may later allow PIE setup with two upgraded Pommel Strike instances, but Phase 8 Automation remains transient-definition based and does not lock production Pommel numeric values.

## Explicit non-goals

```text
repeatable upgrade / UpgradeCount / Searing Blow
UpgradedDescriptionOverride
effect count/type structural replacement
universal Upgrade Delta / Upgrade Context
card-name '+' suffix
Armaments content implementation
Phase 8 implementation
save/load/run-deck persistence
campfire/reward/shop upgrade UX
```

## Next exact action

```text
Implement R1 from docs/CardUpgradeSTSStyleRefactor.md:
→ add UpgradedCost and typed per-effect Upgraded* fields
→ add bool-only effective helpers + validation
→ keep old serialized fields temporarily for six-asset parity migration
→ do not switch/remove old runtime authority until parity authoring is complete
```
