# Phase 7D Validation — Relic Read / Frozen / Native UI

Date: 2026-09-03

## Status

```text
Phase 7D: COMPLETE / VALIDATED / SEALED
C++ implementation: COMPLETE
Development Editor Build: PASS
Focused Automation: 3/3 PASS
Native UMG asset wiring: PASS
Manual PIE visual acceptance: PASS
```

Validated implementation head for the original C++ Read/Frozen gate:

```text
abc02e2d0ec6875b5f1b29192929c2825f0abef5
```

The later Native hover-input correction moved Relic hover ownership onto the explicit transparent `Btn_RelicInteraction` hit layer. This is presentation-only and does not alter the sealed Read/Frozen, FinalSnapshot or Gameplay contracts.

## User-reported C++ gate evidence

The local UE 5.8 Development Editor build passed after the compile-only C4458 local-name shadowing correction in `BattleRelicStripWidget.cpp`.

Focused Automation:

```text
SlayTheSpireDemo.Phase7.RelicPresentation.ReadAndFrozenSnapshot        PASS
SlayTheSpireDemo.Phase7.RelicPresentation.FreezeContract               PASS
SlayTheSpireDemo.Phase7.RelicPresentation.FinalSnapshotReconciliation  PASS

SlayTheSpireDemo.Phase7.RelicPresentation                              3/3 PASS
```

## Contracts validated by the C++ gate

- authoritative Relic runtime facts are exposed through the read-only `FRelicReadView` boundary;
- frozen `FBattleHUDRelicView` contains no `URelicInstance` or other mutable Gameplay runtime pointer;
- frozen Relics are ordered deterministically by RuntimeSequence;
- counter visibility is data-driven through `bShowCounter / Counter / CounterMax`, not `RelicId` checks;
- no first-version `RelicCounterChanged` or `RelicTriggered` Presentation Record is required;
- while an A2 Envelope is playing, the ViewModel retains the last completed historical Relic counter;
- the exact committed counter appears only when the Controller applies `Envelope.FinalSnapshot`;
- the Sundial threshold case is locked specifically as visible historical `2` during `EnergyChanged(+2)` playback, then `0` after FinalSnapshot reconciliation.

## Native UMG wiring — ACCEPTED

The production Native Relic UI is wired through:

```text
WBP_BattleRelic_Native        : parent UBattleRelicWidget
WBP_BattleRelicTooltip_Native : parent UBattleRelicTooltipWidget
WBP_BattleRelicStrip_Native   : parent UBattleRelicStripWidget
WBP_BattleHUD_Native          : embeds RelicStrip_Player
DA_Relic_Sundial              : bShowCounter=true, CounterDisplayMax=3, authored Icon
```

Hover input is owned by the explicit transparent `Btn_RelicInteraction` layer. The owning HUD's `RelicStrip_Player` must remain `Not Hit-Testable (Self Only)`, not `Not Hit-Testable (Self & All Children)`, so child Relic interaction widgets remain in the Slate hit-test path.

The tooltip remains presentation-only and consumes the frozen `FBattleHUDRelicView`; it does not query Gameplay runtime state.

## User-reported manual PIE acceptance — PASS

The following 7D visual/input behaviors were confirmed in PIE on 2026-09-03:

```text
1. Relic strip renders normally.
2. Sundial shows only the current integer counter: 0 / 1 / 2; never /3.
3. Hovering the Relic shows the custom tooltip.
4. Tooltip shows Relic DisplayName + Description.
5. Tooltip follows the mouse cursor.
6. Mouse leave removes the tooltip.
7. No duplicate tooltip or stuck-hover behavior was observed.
8. On the third shuffle, A2 playback retains historical counter 2;
   after the same Envelope FinalSnapshot is applied, the visible counter becomes 0.
```

This confirms both the Native hover/input contract and the already-tested historical-versus-current FinalSnapshot timing in the real production HUD path.

## Non-blocking visual polish

Additional Relic art assets exist under the project's Relic texture area, including outline artwork. Using those outlines for richer icon presentation is optional visual polish only. It does not reopen or block the sealed Phase 7D contract.

## Seal

Phase 7D is complete, validated and sealed.

Future work must not reopen the 7D Gameplay/Read/Frozen/FinalSnapshot contracts for cosmetic Relic UI changes. Any later Relic trigger/effect composition refactor is a separate follow-up design slice and is not part of 7D.
