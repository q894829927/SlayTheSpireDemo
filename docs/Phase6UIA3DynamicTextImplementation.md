# Phase 6UI-A3 Dynamic Text Source Slice

Status:

```text
SOURCE CHANGED
UE5.8 BUILD PASSED AFTER SOURCE FIX
AUTOMATION OWNER RUN: 7/8 TWICE; literal expected-error matching fixed, 8/8 RERUN PENDING
DATAASSET AUTHORING PENDING
PIE REVALIDATION PENDING
```

The second owner run still reported the two intentional fail-soft Error logs.
UE5.8 treats `AddExpectedError` patterns as regular expressions by default, so
the literal `[` / `]` characters did not match. The regression now uses
`AddExpectedErrorPlain`; this is a test-harness correction only.

## Boundary

`UCardData::Description` and `UStatusData::Description` keep their serialized
property names, but are authored as `FText::Format` patterns. Gameplay-side
read-only resolvers produce final FText inside `TryBuildPlayerFacingReadSnapshot`.
The ViewModel and UMG do not calculate Strength, Weak, Vulnerable, Dexterity or
Frailty.

```text
CardEffect / Status Modifier
→ named deterministic value
→ DamageSpec / BlockSpec + existing Pipeline where applicable
→ final description at BattleId + StateRevision
→ BattleHUDViewModel
→ Widget display
```

Enemy-target cards deliberately omit the concrete Enemy target while resolving
their card-face Damage. This includes player Source modifiers but excludes a
particular enemy's target modifiers. Self-target cards resolve Player as both
Source and Target.

Preview resolution does not Commit, enqueue Actions, emit Events, consume RNG or
mutate runtime state. Invalid, missing or duplicate format arguments render `?`
and invalidate the owning DataAsset through editor validation.

## UE Editor asset authoring required

Author these values in UE Editor; do not rewrite the binary assets with text tools.
Localized wording may vary, but argument names must match exactly.

```text
DA_Card_Strike
Description Format = Deal {Damage} damage.
Damage Effect.DescriptionArgumentName = Damage

DA_Card_PommelStrike
Description Format = Deal {Damage} damage. Draw {Draw} card.
Damage Effect.DescriptionArgumentName = Damage
Draw Effect.DescriptionArgumentName = Draw

DA_Card_Defend
Description Format = Gain {Block} Block.
Block Effect.DescriptionArgumentName = Block

DA_Card_Uppercut
Description Format = Deal {Damage} damage. Apply {Weak} Weak and {Vulnerable} Vulnerable.
Damage Effect.DescriptionArgumentName = Damage
Weak ApplyStatus Effect.DescriptionArgumentName = Weak
Vulnerable ApplyStatus Effect.DescriptionArgumentName = Vulnerable

DA_Status_Strength
Description Format = Attack damage +{DamageBonus}.
DamageFlatAdd.DescriptionArgumentName = DamageBonus

DA_Status_Weak
Description Format = Attack damage reduced {DamageReductionPercent}%.
DamageRatio.DescriptionArgumentName = DamageReductionPercent

DA_Status_Vulnerable
Description Format = Attack damage received increased {DamageIncreasePercent}%.
DamageRatio.DescriptionArgumentName = DamageIncreasePercent

DA_Status_Dexterity
Description Format = Block gained +{BlockBonus}.
BlockFlatAdd.DescriptionArgumentName = BlockBonus

DA_Status_Frailty
Description Format = Block gained reduced {BlockReductionPercent}%.
BlockRatio.DescriptionArgumentName = BlockReductionPercent
```

Run **Validate Assets** on these Card/Status DataAssets after authoring. Any missing,
duplicate or unknown argument must be fixed before PIE validation.

## Regression source

`Source/SlayTheSpireDemoTests/Private/Phase6UIA3DynamicTextTests.cpp` contains 8
tests covering source-side Damage, self Block, multi-effect arguments, runtime
Status Amount, ratio percentages, read-only behavior, fail-soft validation and
revision-scoped snapshot refresh.

The owner-only UI workflow now expects:

```text
Phase6UIA3 = 8
Current gated total = 92
```
