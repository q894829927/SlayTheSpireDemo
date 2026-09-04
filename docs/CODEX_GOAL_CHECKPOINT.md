# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-05**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
R3 COMPLETE / VALIDATED
R4 LEGACY SOURCE REMOVAL BUILD PASS / SIX-ASSET LOAD PASS / FINAL RESAVE-PIE-SEAL PENDING

Previous FCardUpgradeConfig Foundation:
HISTORICAL / SUPERSEDED / SOURCE FIELDS REMOVED

Production Card Authoring:
BLOCKED ONLY BY POST-REMOVAL RESAVE + FINAL PRESENTATION PIE/SEAL
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

## R4 source removal

R4 source cleanup is implemented and Build PASS:

```text
removed FCardUpgradeConfig
removed bHasUpgrade
removed UCardData::Upgrade
```

All six production card assets have successfully opened after removal. `UCardVariantData` remains temporarily as a hidden load-compatibility shim until the post-removal resave/commit is complete.

## Upgraded title presentation

Latest explicit presentation requirement:

```text
Gameplay/DTO DisplayName remains the authored base name.
UBattleCardWidget renders upgraded instances as "DisplayName+".
Upgraded title color uses the bright yellow-green reference color (sRGB #7FFF00).
```

This is presentation-only and does not add a second authored upgraded name.

## Next exact actions

```text
1. Build the new BattleCardWidget title presentation change.
2. Run SlayTheSpireDemo.Phase6UIA2N.R4 once (directly invalidated Widget gate).
3. In WBP_BattleCard_Native, verify the inherited UpgradedNameColor is #7FFF00; if Blueprint has an old override, reset it to parent/default or set #7FFF00 manually.
4. PIE with DA_Card_PommelStrike in the opening Hand.
5. Use Test Upgrade First Hand Card.
6. Verify title becomes Pommel Strike+ / 剑柄打击+ in bright yellow-green and numeric text changes to upgraded values.
7. Save/commit post-removal card assets if not already committed.
8. Assess/remove UCardVariantData shim only after resave evidence, then final Build/load check and seal.
```

R3 CardUpgrade / DynamicText / ImmediatePreview / Phase6C remain sticky unless a new change directly invalidates them.

## Explicit non-goals

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
