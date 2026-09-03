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
7D Relic Read/Frozen contract: C++ VALIDATED
7D Relic Native hover UI amendment: IMPLEMENTED / BUILD VALIDATION PENDING / ASSET PENDING

Post-seal card-face continuity correction:
COMPLETE / VALIDATED
```

Active authority:

```text
docs/Phase7RelicsImplementation.md
docs/Phase7DRelicHoverUIAmendment.md   // later explicit 7D visible-UX refinement
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

## 7D read/frozen contract — VALIDATED

```text
URelicInstance runtime facts
→ FRelicReadView
→ FBattleReadSnapshot.Relics
→ RelicPresentationSnapshot::TryFreeze
→ FBattleHUDRelicView
→ FPresentationStateSnapshot.Player.Relics
→ UBattleHUDViewModel.Player.Relics by value
```

`FBattleHUDRelicView` retains no `URelicInstance` or other mutable Gameplay runtime pointer. Relic display order is deterministic by RuntimeSequence.

The user reported a successful Development Editor Build and:

```text
SlayTheSpireDemo.Phase7.RelicPresentation    3/3 PASS
```

This validates Read/Frozen mapping plus FinalSnapshot counter reconciliation. The later hover-UI amendment changes only Native Widget presentation and therefore requires a new build/PIE gate, not a re-design of the Read/Frozen contract.

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

The focused FinalSnapshot reconciliation test locks the Sundial `2 -> 0` case while `EnergyChanged(+2)` is still being presented.

## 7D Native hover UI amendment

The compact visible contract is now:

```text
no-counter Relic
→ icon only

counter Relic
→ icon + lower-right current integer badge
→ 0 / 1 / 2 as individual values
→ never "0/3", "1/3" or "2/3"

hover Relic icon
→ one custom transient tooltip
→ frozen DisplayName + Description
→ follows mouse with a small cursor offset

mouse leave / Relic Widget destruction
→ tooltip removed
```

`CounterMax` remains frozen metadata but is not rendered in the steady-state badge. `bShowCounter` remains the only counter-visibility switch; there is no `RelicId == Sundial` UI branch.

Native class split:

```text
UBattleRelicWidget
→ Img_RelicIcon
→ Txt_RelicCounter
→ TooltipWidgetClass
→ hover / mouse-move / leave lifecycle

UBattleRelicTooltipWidget
→ Txt_RelicName
→ Txt_RelicDescription
→ frozen FBattleHUDRelicView only

UBattleRelicStripWidget
→ unchanged
→ reuses widgets by (RelicId, RuntimeSequence)
```

The old standard `SetToolTipText` path and the steady-state `Txt_RelicName` binding are removed from `UBattleRelicWidget`.

## Production Sundial asset — 7D fields still pending

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
Icon = authored/temporary visual asset
```

`CounterDisplayMax` is presentation metadata only; Gameplay authority remains the trigger's `ShufflesRequired`.

## Next exact action

```text
USER ACTION REQUIRED — UPDATED C++ GATE

1. git pull
2. regenerate UE project files (new reflected Tooltip class was added)
3. Development Editor Build once
4. if Build passes, run SlayTheSpireDemo.Phase7.RelicPresentation once
5. report Build + Automation result
```

Do not create/update the Native Relic WBP assets until this updated C++ gate passes. After it passes, create only:

```text
WBP_BattleRelicTooltip_Native
WBP_BattleRelic_Native
WBP_BattleRelicStrip_Native
```

and embed the strip into `WBP_BattleHUD_Native`. Do not modify Legacy UI.
