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
7C Sundial + GainEnergyAction: IMPLEMENTED / SHARED ENERGY RECORD HELPER IMPLEMENTED / VALIDATION PENDING
7D Relic Read/Frozen/Native UI: NOT STARTED

Post-seal card-face continuity correction:
COMPLETE / VALIDATED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
```

## Accepted 7C gameplay evidence

Bulk Draw is now the durable Draw-N contract:

```text
UDrawCardEffect(DrawCount=N)
→ UDrawCardsAction(N)
   ├─ UDrawCardAction                 // atomic one-card DrawPile -> Hand
   ├─ UShuffleDeckAction              // committed shuffle + FDeckShuffledEvent
   └─ UDrawCardsAction(Remaining)     // continue same bulk request
```

A fresh request against `Draw=0 / Discard=0` ends without shuffle. A previously planned ShuffleAction may later execute with both piles empty and commit `MovedCardCount=0`; this is the generic behavior required for the two-Pommel-Strike+/Sundial infinite and contains no card/relic special case.

User-reported focused gameplay results:

```text
SlayTheSpireDemo.Phase6C                         PASS
SlayTheSpireDemo.Phase7.Sundial                 PASS
```

These gates remain sticky unless Draw/Shuffle/Sundial code changes.

## Post-seal card-face continuity correction — CLOSED

The Draw-to-Hand `red -> white -> red` defect was caused by two independent `FPresentationCardSnapshot -> FBattleHUDCardView` copiers. The Controller reducer copier dropped `RichDescription` between transient Draw playback and FinalSnapshot reconciliation.

The durable boundary is now:

```text
FPresentationCardSnapshot
→ PresentationCardView::MakePresentationOnlyCardView
→ FBattleHUDCardView
```

The mapper is presentation-only, preserves frozen `RichDescription`, forces `bGameplayPlayable=false`, and must not be used for the independent formal current-state path:

```text
FCardReadView
→ ABattleManager::TryFreezePresentationStateSnapshot
→ FBattleHUDCardView with current Gameplay legality
```

`UBattleHUDWidgetBase::MakePresentationCardView()` retains its BlueprintPure API and delegates to the shared mapper. `UBattlePresentationController` uses the same mapper for `CardZoneChanged DrawPile -> Hand` WorkingSnapshot reduction. `RichDescription` is intentionally not added to generic CardPlayed Hand identity comparison because target-specific committed RichText may legitimately differ from the source-side Hand baseline.

User-reported validation on **2026-09-03**:

```text
SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper     PASS
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged          PASS
Focused Strength Draw PIE visual continuity                  PASS
Visible red -> white -> red regression                       NOT OBSERVED
```

Detailed evidence is recorded in `docs/PostSealCardFaceContinuityValidation.md`.

## Shared EnergyChanged Presentation helper

The remaining 7C design-conformance item has now been implemented narrowly.

Shared production helper:

```text
Source/SlayTheSpireDemo/Presentation/EnergyPresentationRecord.h/.cpp

EnergyPresentationRecord::AppendCommittedEnergyChanged(
    const FEnergyCommitResult&,
    const FPresentationRecordWriter&)
```

It owns only the existing committed A2 `EnergyChanged` record semantics:

```text
failed/no-op commit or unavailable writer -> no record
Delta must equal EnergyAfter - EnergyBefore
invalid Delta -> invalidate current Presentation resolution
valid commit -> one EnergyChanged(Before, After, Delta)
append failure -> log; Gameplay remains authoritative
```

Gameplay mutation remains entirely in `BattleEnergyMutation`.

Both active production paths now delegate to this helper:

```text
UGainEnergyAction
→ BattleEnergyMutation::TryGain
→ EnergyPresentationRecord::AppendCommittedEnergyChanged

ABattleManager turn-end / turn-start energy presentation
→ BattleEnergyMutation::SetValue
→ ABattleManager narrow wrapper
→ EnergyPresentationRecord::AppendCommittedEnergyChanged
```

No general Presentation framework, queue behavior, Sundial logic, Energy clamp rule, or Gameplay mutation semantics were changed.

## Required validation gate

New source files were added and the BattleManager/GainEnergyAction EnergyChanged paths changed. Run only:

```text
1. Regenerate project files once.
2. Development Editor Build once.
3. SlayTheSpireDemo.Phase7.EnergyGain once; expected 2/2 PASS.
4. SlayTheSpireDemo.Phase6UIA2C.Record.EndTurnEnergy once; expected 1/1 PASS.
5. Record evidence and STOP.
```

No PIE is required for this helper-only refactor. Do not rerun Sundial, Phase6C, card-face continuity, Phase6R, A2D5, Shipping, Legacy parity, or unrelated UI suites unless a concrete failure invalidates them.

## Production Sundial asset

Expected production/debug asset remains:

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2
```

Icon/HUD display remains 7D.

## Next exact action

```text
USER ACTION REQUIRED:
regenerate project files,
build current main,
run Phase7.EnergyGain and Phase6UIA2C.Record.EndTurnEnergy once each.
```

If those gates pass, record the exact evidence and close 7C implementation acceptance before starting 7D.
