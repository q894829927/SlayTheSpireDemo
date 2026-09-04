# Codex Goal Checkpoint — Card Expansion

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
Phase 7A–7F: COMPLETE / VALIDATED / SEALED

Phase 8 Combo Architecture Validation:
DESIGN REFINED / DEFERRED / NOT A BLOCKER FOR CARD EXPANSION

Card Upgrade STS-Style Refactor:
R1 BUILD PASS / R2 SIX-ASSET PARITY COMMITTED / R3 SOURCE SWITCH IMPLEMENTED / VALIDATION PENDING

Previous FCardUpgradeConfig Foundation:
HISTORICAL / SUPERSEDED / MIGRATION-ONLY FIELDS STILL PRESENT UNTIL R4

Production Card Authoring:
BLOCKED BY R3 VALIDATION + R4 LEGACY-FIELD REMOVAL/RESAVE
```

Current R3 source head:

```text
ef8ea8b5dd17d4956143bac5616427584fca2fb1
```

## Current authority

```text
docs/CardUpgradeSTSStyleRefactor.md
```

## Completed migration state

R1 added and validated the new typed authoring surface:

```text
UCardData.BaseCost / UpgradedCost
Damage: BaseAmount / UpgradedAmount, HitCount / UpgradedHitCount
Block: BaseAmount / UpgradedAmount
Draw: DrawCount / UpgradedDrawCount
ApplyStatus: Amount / UpgradedAmount
```

R2 is user-complete: all six production card assets were parity-authored, saved and committed while the legacy fields still existed:

```text
DA_Card_Strike
DA_Card_Defend
DA_Card_PommelStrike
DA_Card_TwinStrike
DA_Card_Uppercut
DA_Card_Inflame
```

## R3 source switch now implemented

Runtime authority now uses the STS-style model:

```text
UCardInstance::bUpgraded = single mutable ordinary-upgrade truth

GetDescriptionFormat() -> Definition->Description
GetCurrentCost()       -> bUpgraded ? UpgradedCost : BaseCost
ResolveDestination()   -> Definition->DefaultDestination
GetEffects()           -> always Definition->Effects
CanUpgrade()           -> valid Definition && !bUpgraded
```

Creation-time upgraded state is supported at the runtime-instance creation boundary:

```cpp
Initialize(UCardData*, RuntimeId, bStartUpgraded)
```

This does not mutate UCardData. In-combat upgrades still go only through `UUpgradeCardAction -> CommitUpgrade()`.

Current Effect consumers now freeze `Context.Card->IsUpgraded()` at the existing build/preview boundary and pass only the bool into typed helpers:

```text
Damage BuildActions / Dynamic Text / A3 -> GetEffectiveAmount/GetEffectiveHitCount
Block  BuildActions / Dynamic Text / A3 -> GetEffectiveAmount
Draw   BuildActions / Dynamic Text      -> GetEffectiveDrawCount
Status BuildActions / Dynamic Text      -> GetEffectiveAmount
```

`BattleTextResolver::ValidateCardDefinition` now validates one Description + one Effects[] composition; each Effect validates both Base and Upgraded authored values.

`CardUpgradeFoundationTests.cpp` has been migrated in place to prove same Effect object identity, one-time `bUpgraded` mutation, Base/Upgraded typed values, creation-time upgraded state, Dynamic Text and frozen Presentation state.

## Migration-only legacy fields still present

Do not delete yet until R3 validates:

```text
bHasUpgrade
FCardUpgradeConfig
Upgrade.*
UCardVariantData compatibility shim
```

They are no longer runtime authority after R3; they remain only to keep serialized migration safety through R4.

## Required R3 validation

Run in this order:

```text
1. SlayTheSpireDemoEditor Win64 Development Build
2. SlayTheSpireDemo.CardUpgrade
3. SlayTheSpireDemo.UIA3.DynamicText
4. SlayTheSpireDemo.UIA3.ImmediatePreview
5. SlayTheSpireDemo.Phase6C
```

Do not run full Phase 6 / Phase 7.

If all R3 gates pass:

```text
→ enter R4
→ remove FCardUpgradeConfig / bHasUpgrade / Upgrade.*
→ reopen and resave all six production card assets
→ decide whether UCardVariantData compatibility shim can be safely removed
→ final Build + any directly invalidated focused gates
→ one focused PIE visual pass: same name text, upgraded name gold, upgraded numeric text correct
→ seal
```

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
