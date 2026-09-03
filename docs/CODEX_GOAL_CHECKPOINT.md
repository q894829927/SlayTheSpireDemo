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
7D Relic Read/Frozen/Native UI: NEXT / NOT STARTED

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

It owns only established committed A2 `EnergyChanged` record semantics:

```text
failed/no-op commit or unavailable writer -> no record
Delta must equal EnergyAfter - EnergyBefore
invalid Delta -> invalidate current Presentation resolution
valid commit -> one EnergyChanged(Before, After, Delta)
append failure -> log; Gameplay remains authoritative
```

Gameplay mutation remains entirely in `BattleEnergyMutation`.

Both production callers delegate to the same helper:

```text
UGainEnergyAction
→ BattleEnergyMutation::TryGain
→ EnergyPresentationRecord::AppendCommittedEnergyChanged

ABattleManager turn-end / turn-start energy presentation
→ BattleEnergyMutation::SetValue
→ ABattleManager narrow wrapper
→ EnergyPresentationRecord::AppendCommittedEnergyChanged
```

No general Presentation framework, queue behavior, Sundial logic, Energy clamp rule, or Gameplay mutation semantics were added or changed by this cleanup.

## Accepted 7C validation evidence — 2026-09-03

User-reported focused results:

```text
SlayTheSpireDemo.Phase6C                            PASS
SlayTheSpireDemo.Phase7.Sundial                    PASS
SlayTheSpireDemo.Phase7.EnergyGain                 2/2 PASS
SlayTheSpireDemo.Phase6UIA2C.Record.EndTurnEnergy  1/1 PASS
```

The final two gates validate both direct users of the shared EnergyChanged helper:

```text
Phase7.EnergyGain
→ GainEnergyAction + exact Before/After/Delta semantics

Phase6UIA2C.Record.EndTurnEnergy
→ BattleManager established EndTurn EnergyChanged semantics
```

7C is therefore **COMPLETE / VALIDATED / SEALED**. No further Phase6R, A2D5, Shipping, Legacy parity, Phase6C, Sundial, card-face-continuity, or unrelated UI rerun is required for this slice.

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

User-reported validation on **2026-09-03**:

```text
SlayTheSpireDemo.Phase6UIA2D4.PresentationCardViewMapper     PASS
SlayTheSpireDemo.Phase6UIA2C.Record.CardZoneChanged          PASS
Focused Strength Draw PIE visual continuity                  PASS
Visible red -> white -> red regression                       NOT OBSERVED
```

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

Icon/HUD display belongs to 7D.

## Next exact action

```text
7D — Relic Read/Frozen/Native UI

Implement only:
- Relic read DTO
- frozen Presentation relic DTO
- ViewModel/HUD relic view with bShowCounter / Counter / CounterMax
- minimal Native relic Widget/container
- FinalSnapshot reconciliation semantics from the sealed Phase 7 design
```

Do not add a first-version `RelicCounterChanged` / `RelicTriggered` Record, A3 Relic prediction, Legacy Relic UI, acquisition/reward/save/shop systems, or RelicId-specific HUD logic.
