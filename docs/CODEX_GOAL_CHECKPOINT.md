# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-04**

## Current status

```text
Phase 6UI-A / A3: COMPLETE / VALIDATED / SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: COMPLETE / VALIDATED / SEALED
7D Relic Read / Frozen / Native UI: COMPLETE / VALIDATED / SEALED
7E Relic Reaction Composition: COMPLETE / VALIDATED / SEALED
```

## Phase 7E authority / evidence

```text
docs/Phase7ERelicCompositionDesign.md   // sealed design contract
docs/Phase7EImplementation.md           // completed implementation status
docs/Phase7EValidation.md               // trusted final validation evidence
```

## Final 7E implementation

```text
URelicEffect + FRelicEffectContext
UGainEnergyRelicEffect
UGainBlockRelicEffect
UDeckShuffledCountTrigger
UAdvanceRelicCounterAction
URelicInstance generic counter-action friend boundary
URelicData generic Trigger/DataValidation traversal
```

Locked behavior:

```text
BuildReactions eagerly builds/freezes RewardActions
任一 Effect build 失败 -> whole reaction fail-closed
CounterAction::Initialize validates prepared reward batch
RewardAction Outer must equal the target Queue
threshold uses AddBatchToFrontPreserveOrder
Counter resets only after dependent batch insertion succeeds
insertion failure -> ResolutionFault and Counter remains unchanged
CounterAction propagates PresentationRecordWriter to nested RewardActions
GainBlockRelicEffect freezes Owner participant identity before execution
Execute revalidates live Relic membership
```

## Production Sundial migration

`DA_Relic_Sundial` is saved as:

```text
Triggers[0] = UDeckShuffledCountTrigger
RequiredCount = 3
Effects[0] = UGainEnergyRelicEffect
Amount = 2
```

The production asset was validated in PIE. The old Sundial-specific implementation has been removed:

```text
USundialTrigger
USundialAdvanceAction
URelicInstance old USundialAdvanceAction friend
```

## Final trusted Phase 7E gates

```text
Development Editor Build                                      PASS
SlayTheSpireDemo.Phase7E focused gates                         PASS
SlayTheSpireDemo.Phase7.Sundial                                PASS
SlayTheSpireDemo.Phase7.EnergyGain                             PASS
SlayTheSpireDemo.Phase7.RelicPresentation                      PASS
Production Sundial PIE                                         PASS

After old-class deletion:
UE project-file regeneration                                   PASS
Development Editor Build                                       PASS
SlayTheSpireDemo.Phase7.Sundial final regression               PASS
Production PIE smoke / no stale old-class asset reference      PASS
```

Detailed evidence is in `docs/Phase7EValidation.md`.

## Next exact action

```text
STOP.

Do not reopen sealed Phase 7A-7E work or automatically start a new phase.
Select and explicitly authorize the next bounded design/implementation goal first.
```

A possible future cleanup discussed but **not authorized or active** is unifying Relic counter presentation metadata so `CounterDisplayMax` does not duplicate a counter mechanic's authoritative `RequiredCount`. That work is outside sealed Phase 7E and must receive its own design/authorization before implementation.
