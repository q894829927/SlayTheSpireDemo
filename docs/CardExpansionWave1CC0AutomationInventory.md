# Card Expansion — Wave 1C-C0 Focused Automation Inventory

Date: **2026-09-08**

Status:

```text
C0-10 PRESENTATION COVERAGE IMPLEMENTED
C0-11 FOCUSED AUTOMATION COVERAGE IMPLEMENTED
LOCAL EXECUTION NOT YET RECORDED
C0 NOT SEALED
```

Authority:

```text
docs/CardExpansionWave1CC0SelectExhaustGeneralization.md
docs/CardExpansionWave1CC0DescriptionCompatibilityAmendment.md
```

This inventory records the final C0-focused Automation surface after C0-10 / C0-11 implementation.

## C0 focused filter

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0
```

Expected focused test count:

```text
20
```

## Authored configuration / Selection / Continuation

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.AuthoredConfig
SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.DuplicateObjectsRejected
SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.PlayerMultiExhaust
SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.CountClampsToCandidates
```

Coverage:

```text
- backward-compatible Player / 1 defaults
- explicit Base / Upgraded mode and count
- generic duplicate object rejection
- Player exactly-N multi-exhaust
- Player count > candidates clamps to all candidates
```

## Gameplay facade / Native exact-N interaction

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.ExactNFacadeCanonicalSubmit
SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.NativeHUDExactNClick
SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.WrongCountRejected
SlayTheSpireDemo.CardExpansion.Wave1CC0.Selection.StaleCandidateRejected
```

Coverage:

```text
- RuntimeId-only exact-N read/submit facade
- duplicate / foreign RuntimeId rejection
- candidate-order canonicalization independent of click order
- Native toggle / exact-N auto-submit
- wrong selected count rejection while mandatory request remains pending
- stale RuntimeId/object candidate mapping rejection
```

## Random selection / deterministic RNG

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.ChooseIndexDeterministic
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.MultiSelectExhaust
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.SameSeedReproducible
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.NoCandidateSkips
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.SingleSelect
SlayTheSpireDemo.CardExpansion.Wave1CC0.Random.CountClampsToCandidates
```

Coverage:

```text
- deterministic domain-neutral Count -> Index helper
- Count 0 / Count 1 helper boundaries do not advance RNG
- Random exactly-N unique selection without replacement
- same seed/state and ordered candidates reproduce membership
- Random never opens pending player UI
- Random no-candidate path is a legal no-op
- Random exact-1 real Effect consumer
- Random count > candidates selects all available candidates
```

## Count-zero continuation / RNG preservation

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0.SelectExhaust.CountZeroNoOpContinues
```

Coverage:

```text
- authored SelectionCount = 0 is a no-op
- no pending Selection
- no Exhaust
- later authored Effect still resolves
- battle RNG state is not consumed by the zero-count Random SelectExhaust Effect
```

## Description / upgrade preview

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0.Description.LegacyOptOutCompatibility
SlayTheSpireDemo.CardExpansion.Wave1CC0.Description.BaseUpgradeCount
SlayTheSpireDemo.CardExpansion.Wave1CC0.Description.DeclaredArgumentMustBeUsed
```

Coverage:

```text
- existing hard-coded Burning Pact wording remains valid without binary migration
- dynamic {Exhaust} uses BaseSelectionCount / UpgradedSelectionCount
- declared dynamic argument must be consumed by the authored description
```

## C0-10 Presentation

```text
SlayTheSpireDemo.CardExpansion.Wave1CC0.Presentation.MultiExhaustRecordOrder
SlayTheSpireDemo.CardExpansion.Wave1CC0.Presentation.MultiExhaustReducer
```

Coverage:

```text
- one committed Hand -> Exhaust record per selected card
- records remain in canonical candidate order even when submit order is reversed
- Exhaust destination index advances once per exact card
- final committed snapshot has the expected Exhaust count / Hand contents
- controller reducer removes each historical Hand card in sequence
- untouched Hand card remains stable
- stale FromIndex on a later multi-exhaust record is rejected
```

These tests lock the Presentation data/reducer contract. They do not replace the required PIE visual checks for the in-place opacity fade.

## Existing sealed Wave 1C regression gate

C0 validation must also rerun the existing sealed Wave 1C suite separately:

```text
SlayTheSpireDemo.CardExpansion.Wave1C
```

Expected sealed Wave 1C regression count:

```text
13 / 13
```

The Wave 1C suite remains a separate seal gate and is not included in the C0 focused count of 20.

## Remaining validation after C0-10 / C0-11

No further C0 capability implementation is authorized by this inventory.

Before C0 may be sealed, the implementation head still requires:

```text
[ ] SlayTheSpireDemoEditor Win64 Development Build PASS
[ ] SlayTheSpireDemo.CardExpansion.Wave1CC0 = 20 / 20 PASS
[ ] SlayTheSpireDemo.CardExpansion.Wave1C = 13 / 13 PASS
[ ] Native HUD PIE Player multi-select PASS
[ ] Native HUD PIE Random multi-select PASS
[ ] Native HUD PIE Burning Pact regression PASS
[ ] user validation / seal confirmation
```
