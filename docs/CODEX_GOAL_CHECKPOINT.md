# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-03**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: COMPLETE / VALIDATED / SEALED
7D Relic Read / Frozen / Native UI: COMPLETE / VALIDATED / SEALED
7E Relic Reaction Composition: IMPLEMENTATION AUTHORIZED / IN PROGRESS
```

## Active authority

```text
docs/Phase7ERelicCompositionDesign.md   // approved design content
docs/Phase7EImplementation.md           // explicit implementation authorization/status
```

The user explicitly authorized Phase 7E implementation on 2026-09-03. `docs/Phase7EImplementation.md` supersedes the pre-authorization status text left at the end of the design document; the reviewed design contracts remain authoritative.

## Implemented on current remote source

```text
URelicEffect + FRelicEffectContext
UGainEnergyRelicEffect
UGainBlockRelicEffect
UDeckShuffledCountTrigger
UAdvanceRelicCounterAction
URelicInstance generic counter-action friend boundary
URelicData generic Trigger/DataValidation traversal
```

Locked behavior in the implementation:

```text
BuildReactions eagerly builds/freeze RewardActions
任一 Effect build 失败 -> whole reaction fail-closed
CounterAction::Initialize returns bool and validates prepared reward batch
RewardAction Outer must equal the target Queue
threshold uses AddBatchToFrontPreserveOrder
Counter resets only after dependent batch insertion succeeds
insertion failure -> ResolutionFault and Counter remains unchanged
CounterAction propagates its PresentationRecordWriter to nested RewardActions
GainBlockRelicEffect freezes Owner participant identity before execution
Execute revalidates live Relic membership
```

## Tests already migrated/added in source

```text
Source/SlayTheSpireDemoTests/Private/Phase7ERelicCompositionTests.cpp
Source/SlayTheSpireDemoTests/Private/Phase7ERelicCounterFaultTests.cpp

Phase7CEnergyAndSundialTests.cpp
- Phase7.Sundial fixture now uses UDeckShuffledCountTrigger + UGainEnergyRelicEffect
- frozen-config test mutates RequiredCount/Effect Amount after BuildReactions

Phase7DRelicPresentationTests.cpp
- Sundial FinalSnapshot fixture now uses the new composition path
```

No Build, Automation or PIE result has been claimed after these changes.

## Next exact gate

```text
USER ACTION REQUIRED

1. git pull
2. regenerate UE project files because new reflected UCLASS headers were added
3. Development Editor Build once
4. STOP and report the build result
```

Use the project-standard commands from root `AGENTS.md`.

If Build passes, the next single gate is:

```text
SlayTheSpireDemo.Phase7E
```

Do not run the Sundial/RelicPresentation regression before the focused 7E suite passes.

## Remaining after focused 7E passes

```text
1. In Unreal Editor migrate production DA_Relic_Sundial:
   Trigger = UDeckShuffledCountTrigger
   RequiredCount = 3
   Effects[0] = UGainEnergyRelicEffect
   Amount = 2
2. Save the .uasset.
3. Run the required migrated Phase7.Sundial / Phase7.EnergyGain / Phase7.RelicPresentation regression once.
4. Only after the production asset and tests no longer depend on old classes:
   delete USundialTrigger / USundialAdvanceAction
   remove the old USundialAdvanceAction friend from URelicInstance
5. Final minimal regression and Phase7E validation/seal documentation.
```

Connected GitHub editing cannot author the existing binary `DA_Relic_Sundial.uasset`, so the old Sundial C++ classes must remain until that Unreal Editor asset migration is saved.
