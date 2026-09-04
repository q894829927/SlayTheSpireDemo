# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
R1 IMPLEMENTED / BUILD PENDING / R2 SIX-ASSET PARITY PENDING

Previous FCardUpgradeConfig Foundation:
HISTORICAL / SUPERSEDED / DO NOT REVALIDATE AS FINAL TARGET

Production Card Authoring:
BLOCKED BY STS-STYLE REFACTOR + SIX-ASSET MIGRATION

Card Trigger Source Expansion:
DESIGN DRAFT / FUTURE INDEPENDENT FOUNDATION SLICE / IMPLEMENTATION NOT AUTHORIZED
```

Current repository HEAD after R1 source edits:

```text
16ce7863c6ae4910ce4c0627dadb0e626641115f
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

Remove as ordinary-card authority after parity:

```text
bHasUpgrade
FCardUpgradeConfig
second Upgrade.Description
second Upgrade.Destination
second Upgrade.Effects[]
```

## R1 completed source work

R1 intentionally adds the new typed authoring surface while preserving the old serialized/runtime authority for migration parity.

Implemented:

```text
UCardData
├─ BaseCost
└─ UpgradedCost

UDamageCardEffect
├─ BaseAmount / UpgradedAmount
├─ HitCount / UpgradedHitCount
├─ GetEffectiveAmount(bool)
└─ GetEffectiveHitCount(bool)

UGainBlockCardEffect
├─ BaseAmount / UpgradedAmount
└─ GetEffectiveAmount(bool)

UDrawCardEffect
├─ DrawCount / UpgradedDrawCount
└─ GetEffectiveDrawCount(bool)

UApplyStatusCardEffect
├─ Amount / UpgradedAmount
└─ GetEffectiveAmount(bool)
```

DataValidation now validates both Base and Upgraded typed values, including BaseCost/UpgradedCost.

The helpers accept only `bool bIsUpgraded`; Effect headers do not depend on `UCardInstance`.

No `Upgraded*=0` fallback semantics were added. Current defaults are valid authored defaults only; production assets must still be parity-authored explicitly in R2.

R1 deliberately did **not** switch current Gameplay/DynamicText/A3 consumers. They still use the legacy `FCardUpgradeConfig` authority until the six production assets have parity values in the new fields.

## Effective-value boundary

Final resolver API remains:

```cpp
GetEffectiveXXX(bool bIsUpgraded)
```

not:

```cpp
GetEffectiveXXX(UCardInstance*)
```

Calling boundary will freeze `Card->IsUpgraded()` and pass only the bool during R3.

Current build-time freeze is legal because the card being played does not change its own `bUpgraded` during that resolution. If a future mechanic can change that state mid-resolution, affected effective-value resolution must move to Action Execute-time.

## Presentation state remains locked

```text
UCardInstance::bUpgraded
→ FPresentationCardSnapshot.bUpgraded
→ FBattleHUDCardView.bUpgraded
→ UBattleCardWidget
→ upgraded name gold
```

DisplayName text itself remains unchanged. No `+` suffix in Gameplay.

## R2 migration scope

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
old model remains authoritative and visible
→ copy/check old Upgrade.Cost into new UpgradedCost
→ copy/check each old Upgrade.Effects value into the matching Base Effects[].Upgraded* field
→ verify effect order/type parity
→ Save the six assets with both old and new data still present
→ only after parity evidence proceed to R3
```

`UCardVariantData` load shim stays until later asset load/resave evidence proves removal is safe.

## Test migration

Do not add a parallel new upgrade test file.

R3 will migrate:

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

## Validation state

No current-head UE build or Automation result is claimed yet.

Before R2 asset authoring, run one editor build so the new reflected properties are known-good and visible in UE Editor.

Final-head gates for the completed refactor remain:

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade
3. SlayTheSpireDemo.UIA3.DynamicText
4. SlayTheSpireDemo.UIA3.ImmediatePreview
5. SlayTheSpireDemo.Phase6C
6. one focused PIE visual pass
```

Phase6C is required because R3 will modify current Effect `BuildActions` authored-value reads.

Do not run full Phase 6 / Phase 7.

## Phase 8 relation

Phase 8 remains deferred and non-blocking.

The future spawn/starting-card upgraded-state spec may allow PIE setup with two upgraded Pommel Strike instances, but Phase 8 Automation remains transient-definition based and does not lock production Pommel numeric values.

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
1. Build current R1 head once.
2. If Build PASS, perform R2 parity authoring on all six DA_Card_* assets while the legacy fields still exist.
3. Report parity complete.
4. Then implement R3: switch runtime consumers/tests to the new typed authority.
```
