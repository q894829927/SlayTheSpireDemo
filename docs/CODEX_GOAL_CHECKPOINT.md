# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
R3 COMPLETE / VALIDATED
R4 LEGACY SOURCE REMOVAL IMPLEMENTED / BUILD + SIX-ASSET RESAVE PENDING

Previous FCardUpgradeConfig Foundation:
HISTORICAL / SUPERSEDED / SOURCE FIELDS REMOVED

Production Card Authoring:
BLOCKED ONLY BY R4 BUILD + POST-REMOVAL ASSET RESAVE + FINAL PIE/SEAL
```

## Current authority

```text
docs/CardUpgradeSTSStyleRefactor.md
```

## Completed migration state

R1 added the typed authoring surface:

```text
UCardData.BaseCost / UpgradedCost
Damage: BaseAmount / UpgradedAmount, HitCount / UpgradedHitCount
Block: BaseAmount / UpgradedAmount
Draw: DrawCount / UpgradedDrawCount
ApplyStatus: Amount / UpgradedAmount
```

R2 user-complete: all six production card assets were parity-authored, saved and committed while the legacy fields still existed:

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

R3 runtime authority now uses the STS-style model:

```text
UCardInstance::bUpgraded = single mutable ordinary-upgrade truth

GetDescriptionFormat() -> Definition->Description
GetCurrentCost()       -> bUpgraded ? UpgradedCost : BaseCost
ResolveDestination()   -> Definition->DefaultDestination
GetEffects()           -> always Definition->Effects
CanUpgrade()           -> valid Definition && !bUpgraded
```

Creation-time upgraded state remains supported via:

```cpp
Initialize(UCardData*, RuntimeId, bStartUpgraded)
```

In-combat upgrades still go only through `UUpgradeCardAction -> CommitUpgrade()`.

Effect consumers converge through typed bool-only helpers:

```text
Damage BuildActions / Dynamic Text / A3 -> GetEffectiveAmount/GetEffectiveHitCount
Block  BuildActions / Dynamic Text / A3 -> GetEffectiveAmount
Draw   BuildActions / Dynamic Text      -> GetEffectiveDrawCount
Status BuildActions / Dynamic Text      -> GetEffectiveAmount
```

## R3 validation evidence

All focused gates PASS:

```text
SlayTheSpireDemoEditor Win64 Development Build
SlayTheSpireDemo.CardUpgrade
SlayTheSpireDemo.Phase6UIA3.DynamicText
SlayTheSpireDemo.UIA3.ImmediatePreview
SlayTheSpireDemo.Phase6C
```

The Dynamic Text suite uses the real prefix `SlayTheSpireDemo.Phase6UIA3.DynamicText`.

## R4 source removal

R4 source cleanup is implemented:

```text
removed FCardUpgradeConfig
removed bHasUpgrade
removed UCardData::Upgrade
```

`UCardVariantData` is intentionally still present as a hidden load-compatibility shim. Do not delete it until the six production assets have successfully loaded and been resaved after legacy field removal.

## Next exact actions

```text
1. Rebuild SlayTheSpireDemoEditor Win64 Development.
2. If Build PASS, open all six DA_Card_* assets.
3. Verify BaseCost / UpgradedCost and each Effect Base/Upgraded value.
4. Confirm old Has Upgrade / Upgrade authoring surface is gone.
5. Save all six assets and commit the resulting .uasset changes.
6. Then assess whether UCardVariantData shim can be removed safely.
7. Perform one focused PIE: normal name color; upgraded same name text in gold; upgraded numeric text correct.
8. Seal.
```

R3 CardUpgrade / DynamicText / ImmediatePreview / Phase6C remain sticky after this R4 source-only legacy removal unless Build or asset loading exposes a shared regression.

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
