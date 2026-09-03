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
7C Sundial + GainEnergyAction: IMPLEMENTED / GAMEPLAY GATES PASS / FINAL DESIGN CONFORMANCE PENDING
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

The CardZoneChanged prefix includes the new `WorkingSnapshotRichContinuity` Stage-B regression. Detailed evidence is recorded in:

```text
docs/PostSealCardFaceContinuityValidation.md
```

No further Phase6C/Sundial/Phase6R/A2D5/Shipping/Legacy regression is required for this correction.

## Remaining 7C design-conformance item

The sealed Phase 7 design requires `BattleManager` and `UGainEnergyAction` to produce `EnergyChanged` Presentation records through one narrowly scoped shared helper when the existing construction is private to `BattleManager.cpp`.

Current `UGainEnergyAction` still constructs the `EnergyChanged` payload locally. This is behaviorally validated but does not yet satisfy that sealed implementation constraint. Do **not** mark 7C SEALED until this duplication is removed.

Required narrow correction:

```text
extract one shared EnergyChanged-record helper
→ BattleManager existing energy-change presentation path delegates to it
→ UGainEnergyAction delegates to it
→ no general Presentation refactor
```

After that correction, invalidate only the directly affected evidence:

```text
Development Editor Build
SlayTheSpireDemo.Phase7.EnergyGain
```

Rerun Sundial only if the helper correction changes Gameplay/queue behavior; a pure record-construction refactor does not by itself invalidate the existing Sundial gameplay gate.

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

The user's real two-Pommel-Strike+/Sundial PIE investigation has already exercised the configured Sundial behavior. Icon/HUD display remains 7D.

## Next exact action

```text
7C: extract the narrow shared EnergyChanged Presentation helper,
then run Editor Build + SlayTheSpireDemo.Phase7.EnergyGain once.
```

Do not begin 7D or mark 7C SEALED before that design-conformance correction is complete and validated.
