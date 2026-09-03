# Phase 7D Validation — Relic Read / Frozen / Native UI

Date: 2026-09-03

## Status

```text
C++ implementation: COMPLETE
Development Editor Build: PASS
Focused Automation: 3/3 PASS
Native UMG asset wiring: PENDING
Manual PIE visual acceptance: PENDING
```

Validated implementation head for the C++ gate:

```text
abc02e2d0ec6875b5f1b29192929c2825f0abef5
```

## User-reported C++ gate evidence

The local UE 5.8 Development Editor build passed after the compile-only C4458 local-name shadowing correction in `BattleRelicStripWidget.cpp`.

Focused Automation:

```text
SlayTheSpireDemo.Phase7.RelicPresentation.ReadAndFrozenSnapshot        PASS
SlayTheSpireDemo.Phase7.RelicPresentation.FreezeContract               PASS
SlayTheSpireDemo.Phase7.RelicPresentation.FinalSnapshotReconciliation  PASS

SlayTheSpireDemo.Phase7.RelicPresentation                              3/3 PASS
```

## Contracts validated by this gate

- authoritative Relic runtime facts are exposed through the read-only `FRelicReadView` boundary;
- frozen `FBattleHUDRelicView` contains no `URelicInstance` or other mutable Gameplay runtime pointer;
- frozen Relics are ordered deterministically by RuntimeSequence;
- counter visibility is data-driven through `bShowCounter / Counter / CounterMax`, not `RelicId` checks;
- no first-version `RelicCounterChanged` or `RelicTriggered` Presentation Record is required;
- while an A2 Envelope is playing, the ViewModel retains the last completed historical Relic counter;
- the exact committed counter appears only when the Controller applies `Envelope.FinalSnapshot`;
- the Sundial threshold case is locked specifically as visible historical `2` during `EnergyChanged(+2)` playback, then `0` after FinalSnapshot reconciliation.

## Remaining 7D acceptance

Only the Native UMG asset slice remains:

```text
WBP_BattleRelic_Native      : parent UBattleRelicWidget
WBP_BattleRelicStrip_Native : parent UBattleRelicStripWidget
WBP_BattleHUD_Native        : embed the Relic Strip
DA_Relic_Sundial            : bShowCounter=true, CounterDisplayMax=3, optional Icon
```

After those assets compile, perform one focused PIE acceptance. 7D is not sealed until that visual gate passes.
