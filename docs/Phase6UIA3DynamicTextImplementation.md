# Phase 6UI-A3 Dynamic Text Source Slice

Status:

```text
A3-1 DYNAMIC TEXT SEALED
UE5.8 BUILD PASS
AUTOMATION OWNER RUN PASS 8/8
DATAASSET AUTHORING COMPLETE
PIE REVALIDATION PASS
PACKAGE REVALIDATION PASS
```

This slice is the completed first part of UI-A3. It remains intentionally separate from the unfinished target-specific/current-state Preview work.

## Boundary

`UCardData::Description` and `UStatusData::Description` keep their serialized property names, but are authored as `FText::Format` patterns. Gameplay-side read-only resolvers produce final FText inside `TryBuildPlayerFacingReadSnapshot`. The ViewModel and UMG do not calculate Strength, Weak, Vulnerable, Dexterity or Frailty.

```text
CardEffect / Status Modifier
→ named deterministic value
→ DamageSpec / BlockSpec + existing Pipeline where applicable
→ final description at BattleId + StateRevision
→ BattleHUDViewModel
→ Widget display
```

Enemy-target cards deliberately omit the concrete Enemy target while resolving their card-face Damage. This includes player Source modifiers but excludes a particular enemy's target modifiers. Self-target cards resolve Player as both Source and Target.

This distinction is now part of the locked A3 roadmap:

```text
Card-face Dynamic Text
= current source-side/self presentation value

A3-2 Target-Specific Current-State Preview
= supported Operation value for one concrete current target at one BattleId/StateRevision
```

Do not call the latter an exact final card result. It does not simulate later commits, trigger/relic reactions or final HP outcomes.

Preview/text resolution does not Commit, enqueue Actions, emit Events, consume RNG or mutate runtime state. Invalid, missing or duplicate format arguments render `?` and invalidate the owning DataAsset through editor validation.

## Authored UE assets

The validated DataAsset argument scheme is:

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

## Regression source

`Source/SlayTheSpireDemoTests/Private/Phase6UIA3DynamicTextTests.cpp` contains 8 tests covering source-side Damage, fixed multi-hit intent, self Block, multi-effect named arguments, runtime Status Amount, ratio percentages, read-only behavior, fail-soft validation and revision-scoped snapshot refresh.

Validated focused evidence:

```text
Phase6UIA3 = 8/8 PASS
```

The historical combined UI owner run that included this slice passed 92/92. Exact totals are evidence for that run, not permanent architecture constants.

## Next A3 work — only after UI-A2E

The mainline does **not** proceed directly from A2D5 to A3-2. First complete `UI-A2E — Unified Blueprint Playback & PIE Acceptance` and seal the post-commit A2 presentation surface.

Then continue:

```text
A3-2 Target-Specific Current-State Preview
A3-3 Energy + Target-Aware Legality
A3-4 ViewModel Transient Preview Lifecycle
A3-5 Minimal UMG + A2/A3 Combined PIE
```

The locked detailed route is `docs/Phase6UIA2EImplementation.md`.
