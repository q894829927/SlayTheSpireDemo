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
7D Relic Read / Frozen / Native UI: COMPLETE / VALIDATED / SEALED

Post-seal card-face continuity correction:
COMPLETE / VALIDATED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
docs/Phase7DRelicHoverUIAmendment.md   // sealed 7D visible-UX refinement
```

Accepted evidence:

```text
docs/Phase7CValidation.md
docs/Phase7DValidation.md
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

```text
Source/SlayTheSpireDemo/Presentation/EnergyPresentationRecord.h/.cpp

EnergyPresentationRecord::AppendCommittedEnergyChanged(
    const FEnergyCommitResult&,
    const FPresentationRecordWriter&)
```

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

## 7D Relic Read / Frozen / Native UI — SEALED

```text
URelicInstance runtime facts
→ FRelicReadView
→ FBattleReadSnapshot.Relics
→ RelicPresentationSnapshot::TryFreeze
→ FBattleHUDRelicView
→ FPresentationStateSnapshot.Player.Relics
→ UBattleHUDViewModel.Player.Relics by value
→ Native Relic Strip / Tooltip
```

`FBattleHUDRelicView` retains no `URelicInstance` or other mutable Gameplay runtime pointer. Relic display order is deterministic by RuntimeSequence.

Accepted C++ evidence:

```text
Development Editor Build                                      PASS
SlayTheSpireDemo.Phase7.RelicPresentation.ReadAndFrozenSnapshot        PASS
SlayTheSpireDemo.Phase7.RelicPresentation.FreezeContract               PASS
SlayTheSpireDemo.Phase7.RelicPresentation.FinalSnapshotReconciliation  PASS
SlayTheSpireDemo.Phase7.RelicPresentation                              3/3 PASS
```

## 7D historical playback contract

No `RelicCounterChanged` / `RelicTriggered` Record exists in the first version.

```text
A2 Envelope active
→ Relic strip observes historical ViewModel snapshot
→ counter remains last completed historical value

Envelope completes
→ Controller applies Envelope.FinalSnapshot
→ exact committed Relic counter becomes visible
```

The focused FinalSnapshot reconciliation test and manual PIE lock the Sundial `2 -> 0` case while `EnergyChanged(+2)` is still being presented.

## 7D Native Relic UI contract — VALIDATED

Steady state:

```text
no-counter Relic
→ icon only

counter Relic
→ icon + lower-right current integer badge
→ 0 / 1 / 2 as individual values
→ never "0/3", "1/3" or "2/3"
```

Hover:

```text
Btn_RelicInteraction
→ OnHovered
→ one custom transient tooltip
→ frozen DisplayName + Description
→ follows mouse with a small cursor offset

OnUnhovered / Relic Widget destruction
→ tooltip removed
```

The production HUD `RelicStrip_Player` must remain `Not Hit-Testable (Self Only)`, not `Not Hit-Testable (Self & All Children)`, so child Relic interaction widgets remain in the Slate hit-test path.

User-reported manual PIE acceptance on **2026-09-03**:

```text
Relic strip renders normally                                      PASS
Sundial shows 0 / 1 / 2 only; never /3                          PASS
Custom hover tooltip appears                                     PASS
Tooltip shows DisplayName + Description                          PASS
Tooltip follows mouse                                            PASS
Mouse leave removes tooltip                                      PASS
No duplicate tooltip / stuck hover                               PASS
Third shuffle: historical 2 during A2, then 0 after FinalSnapshot PASS
```

7D is **COMPLETE / VALIDATED / SEALED**.

## Production Sundial asset — ACCEPTED

```text
DA_Relic_Sundial : URelicData
RelicId = Sundial
DisplayName = 日晷
Description = 每洗牌3次，获得2点能量。
Triggers[0] = USundialTrigger
    ShufflesRequired = 3
    EnergyGain = 2

bShowCounter = true
CounterDisplayMax = 3
Icon = authored Relic visual asset
```

`CounterDisplayMax` is presentation metadata only; Gameplay authority remains the trigger's `ShufflesRequired`.

Additional Relic texture assets, including outline artwork under the Relic texture area, are non-blocking cosmetic polish and do not reopen 7D.

## Next exact action

```text
STOP — Phase 7D is sealed.

Do not reopen 7D Gameplay/Read/Frozen/FinalSnapshot contracts for cosmetic changes.
Any future Relic Trigger/Effect composition refactor must be designed and gated as a separate follow-up slice.
```
