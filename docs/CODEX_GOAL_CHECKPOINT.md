# Codex Goal Checkpoint — Phase 7 Relics

Last updated: **2026-09-03**

## Goal

Implement Phase 7 Relics as a first-class deterministic Gameplay system, beginning with Sundial, without reopening sealed Phase 6UI-A Gameplay architecture or modeling Relics as Statuses.

## Current status

```text
Phase 6UI-A: COMPLETE / VALIDATED / SEALED
Phase 6UI-A3: COMPLETE / VALIDATED / SEALED

Phase 7 Relics: IN PROGRESS
Phase 7 design: SEALED
7A Relic Runtime: COMPLETE / VALIDATED / SEALED
7B Status + Relic Trigger Sources: COMPLETE / VALIDATED / SEALED
7C Sundial + GainEnergyAction: COMPLETE / VALIDATED / SEALED
7D Relic Read/Frozen/Native UI: IMPLEMENTED / C++ VALIDATION PENDING / ASSET PENDING

Post-seal card-face continuity correction:
COMPLETE / VALIDATED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

7C accepted evidence:

```text
docs/Phase7CValidation.md
```

Post-seal card-face continuity evidence:

```text
docs/PostSealCardFaceContinuityValidation.md
```

## Sealed 7C gameplay contract

Bulk Draw is the durable Draw-N contract:

```text
UDrawCardEffect(DrawCount=N)
→ UDrawCardsAction(N)
   ├─ UDrawCardAction                 // atomic one-card DrawPile -> Hand
   ├─ UShuffleDeckAction              // committed shuffle + FDeckShuffledEvent
   └─ UDrawCardsAction(Remaining)     // continue same bulk request
```

A fresh request against `Draw=0 / Discard=0` ends without shuffle. A previously planned ShuffleAction may later execute with both piles empty and commit `MovedCardCount=0`; this is the generic behavior required for the two-Pommel-Strike+/Sundial infinite and contains no card/relic special case.

Sundial remains event-driven and source-neutral:

```text
committed authoritative FDeckShuffledEvent
→ read-only USundialTrigger
→ USundialAdvanceAction with frozen RequiredShuffles / EnergyGain
→ authoritative counter mutation
→ every third shuffle resets counter
→ dependent UGainEnergyAction(+2)
```

Positive Energy mutation remains:

```text
BattleEnergyMutation::TryGain
→ UGainEnergyAction
```

The first-version contract permits temporary Energy above `MaxEnergy`; zero, negative, overflow and invalid-Battle requests fail soft.

## Shared EnergyChanged Presentation helper — CLOSED

The final 7C design-conformance item is complete.

Shared helper:

```text
Source/SlayTheSpireDemo/Presentation/EnergyPresentationRecord.h/.cpp

EnergyPresentationRecord::AppendCommittedEnergyChanged(
    const FEnergyCommitResult&,
    const FPresentationRecordWriter&)
```

It owns only established committed A2 `EnergyChanged` record semantics. Gameplay mutation remains entirely in `BattleEnergyMutation`.

Accepted user-reported focused results on 2026-09-03:

```text
SlayTheSpireDemo.Phase6C                            PASS
SlayTheSpireDemo.Phase7.Sundial                    PASS
SlayTheSpireDemo.Phase7.EnergyGain                 2/2 PASS
SlayTheSpireDemo.Phase6UIA2C.Record.EndTurnEnergy  1/1 PASS
```

7C is **COMPLETE / VALIDATED / SEALED**.

## Post-seal card-face continuity correction — CLOSED

The durable card projection boundary is:

```text
FPresentationCardSnapshot
→ PresentationCardView::MakePresentationOnlyCardView
→ FBattleHUDCardView
```

User-reported validation on **2026-09-03**:

```text
SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper     PASS
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged          PASS
Focused Strength Draw PIE visual continuity                  PASS
Visible red -> white -> red regression                       NOT OBSERVED
```

## 7D implementation checkpoint

The C++ implementation for the Relic read/frozen/native-display boundary is now present but has not yet been validated by the local UE 5.8 build.

### Read and frozen state

```text
URelicInstance runtime facts
→ FRelicReadView
   - weak Relic observation handle
   - weak immutable Definition handle
   - RelicId
   - RuntimeSequence
   - Counter
→ player-facing FBattleReadSnapshot.Relics
→ RelicPresentationSnapshot::TryFreeze
→ FBattleHUDRelicView
   - RelicId
   - RuntimeSequence
   - DisplayName
   - Description
   - bShowCounter
   - Counter
   - CounterMax
   - immutable Icon
→ FPresentationStateSnapshot.Player.Relics
→ UBattleHUDViewModel.Player.Relics by value
```

`FBattleHUDRelicView` retains no `URelicInstance` or other mutable Gameplay runtime pointer. Relic display order is deterministic by RuntimeSequence.

### Counter presentation metadata

`URelicData` now carries immutable presentation metadata only:

```text
Icon
bShowCounter
CounterDisplayMax
```

`CounterDisplayMax` does not drive Sundial Gameplay. The trigger's authored `ShufflesRequired` remains Gameplay authority. Native UI decides counter visibility from frozen `bShowCounter`, never from `RelicId`.

### Historical playback contract

No `RelicCounterChanged` / `RelicTriggered` Record was added.

```text
A2 Envelope active
→ Relic strip observes historical ViewModel snapshot
→ counter remains last completed historical value

Envelope completes
→ Controller applies Envelope.FinalSnapshot
→ exact committed Relic counter becomes visible
```

The focused `FinalSnapshotReconciliation` test explicitly locks the Sundial `2 -> 0` case while an `EnergyChanged(+2)` Record is playing.

### Native UI boundary

Two Native-only widgets were added:

```text
UBattleRelicWidget
→ consumes one FBattleHUDRelicView only
→ name / optional icon / data-driven counter / frozen description tooltip

UBattleRelicStripWidget
→ embedded child of WBP_BattleHUD_Native
→ binds to the owning HUD's existing frozen ViewModel
→ rebuilds only when Relic identity/order changes
→ otherwise reuses widgets by (RelicId, RuntimeSequence) and updates frozen views in place
```

The strip does not query BattleManager, Relic runtime, or definition objects.

### Focused 7D Automation added

```text
SlayTheSpireDemo.Phase7.RelicPresentation.ReadAndFrozenSnapshot
SlayTheSpireDemo.Phase7.RelicPresentation.FreezeContract
SlayTheSpireDemo.Phase7.RelicPresentation.FinalSnapshotReconciliation
```

Expected prefix result after a successful build:

```text
SlayTheSpireDemo.Phase7.RelicPresentation    3/3 PASS
```

## Production Sundial asset — 7D fields still pending

The existing Sundial definition remains:

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2
```

After the C++ gate passes, the 7D asset step must additionally set:

```text
bShowCounter = true
CounterDisplayMax = 3
Icon = optional
```

## Next exact action

```text
USER ACTION REQUIRED — C++ GATE ONLY

1. regenerate UE project files
2. Development Editor Build once
3. run SlayTheSpireDemo.Phase7.RelicPresentation once
4. report Build result and Automation result
```

Do not create/edit WBP assets until this C++ gate passes. After it passes, create only the Native Relic Widget/Strip assets and embed the strip into WBP_BattleHUD_Native. Do not modify Legacy UI.
