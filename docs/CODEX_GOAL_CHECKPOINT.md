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

The user explicitly authorized Phase 7E implementation on 2026-09-03. `docs/Phase7EImplementation.md` supersedes the pre-authorization status text left at the end of the design document; the design contracts themselves remain authoritative.

## 7E locked implementation contract

```text
URelicData.Triggers[]
→ UDeckShuffledCountTrigger
→ URelicEffect[]
→ prepared RewardActions built during BuildReactions
→ UAdvanceRelicCounterAction
→ threshold uses AddBatchToFrontPreserveOrder
→ RewardActions inherit CounterAction PresentationRecordWriter
→ authoritative Queue executes rewards
```

First-version effects:

```text
UGainEnergyRelicEffect
UGainBlockRelicEffect
```

Not in 7E first version:

```text
UDrawCardsRelicEffect
new BattleEvent kinds
GenericEventTrigger / condition DSL
7D UI or Frozen DTO changes
```

## Implemented so far

```text
Source/SlayTheSpireDemo/Relics/Effects/RelicEffect.h
Source/SlayTheSpireDemo/Relics/Effects/GainEnergyRelicEffect.h/.cpp
Source/SlayTheSpireDemo/Relics/Effects/GainBlockRelicEffect.h/.cpp
Source/SlayTheSpireDemo/Actions/AdvanceRelicCounterAction.h/.cpp
Source/SlayTheSpireDemo/Relics/DeckShuffledCountTrigger.h/.cpp
RelicData generic child-trigger validation
URelicInstance friend boundary extended to UAdvanceRelicCounterAction
Source/SlayTheSpireDemoTests/Private/Phase7ERelicCompositionTests.cpp
```

No Build or Automation result has been claimed yet.

## Remaining exact sequence

```text
1. Source-review the new 7E primitives/tests for compile-contract issues.
2. User pulls/regenerates project files and runs Development Editor Build once.
3. If Build passes, run SlayTheSpireDemo.Phase7E once.
4. Fix only failed 7E gate if needed.
5. Migrate Phase7.Sundial / Phase7.RelicPresentation test fixtures to the new Trigger+Effect path.
6. In Unreal Editor migrate production DA_Relic_Sundial:
   UDeckShuffledCountTrigger RequiredCount=3
   Effects[0]=UGainEnergyRelicEffect Amount=2
7. Run required Sundial / EnergyGain / RelicPresentation regression.
8. Only after asset + tests no longer reference old classes, delete USundialTrigger / USundialAdvanceAction and remove their RelicInstance friend.
9. Final minimal regression, then record Phase7E validation/seal.
```

## Binary asset boundary

Connected GitHub editing cannot author the existing `DA_Relic_Sundial.uasset`. Do not delete the old Sundial C++ classes before the Unreal Editor asset migration is saved, or the production asset can retain a broken class reference.
