# Phase 6UI-A2N — R3-A Review Fix Validation

Status: **COMPLETE / VALIDATED**

Branch: `a2n/r3-review-fix`
Base: `main` at `41ca570edf820b418a900c8826d4c424538325d4`

This review fix closes the missing Native combatant-inspection tooltip behavior and the zero-Block badge parity bug without entering R4 Hand/Card ownership, R7 Damage playback, or R9 status-row lifecycle ownership.

## Code fixes

### Frozen status tooltip

`UBattleHUDWidget::HandleCombatantInspectRequested` now forwards the selected combatant's frozen `FBattleHUDCombatantView::Statuses` array to the existing optional Designer tooltip function:

```text
RebuildTooltip(TArray<FBattleHUDStatusView>)
```

The bridge uses only the frozen ViewModel DTO. It does not create, update, remove, identify, or reconcile formal status-row widgets. Those rules remain reserved for R9.

If the optional tooltip surface does not expose the expected `RebuildTooltip` function, the Native HUD fails closed for that optional surface and keeps it collapsed rather than showing stale tooltip content.

### Zero-Block badge visibility

The Native static combatant refresh now collapses the complete existing Designer Block badge when the frozen `Combatant.Block` value is zero, and restores it when Block is positive.

The implementation reuses the sealed Designer hierarchy:

```text
Txt_PlayerBlock -> OV_PlayerBlock -> SB_PlayerBlockBadge
Txt_EnemyBlock  -> OV_EnemyBlock  -> SB_EnemyBlockBadge
```

No new BindWidget member was added, so the R2 23-required / 6-optional binding contract is unchanged. The badge visibility is driven only from the frozen `FBattleHUDCombatantView::Block` value.

## Focused Editor-only Automation

A permanent Editor-only focused suite exercises the four R3 review contracts without requiring Hand/Card migration, real combat completion, Damage playback, or formal Status-row lifecycle:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable
```

The test probes live only in `SlayTheSpireDemoTests` and production runtime does not depend on them.

The user ran the saved branch head locally on UE 5.8 after rebuilding `SlayTheSpireDemoEditor`. The build completed successfully and the full prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R3
```

completed with all four focused tests passing.

## Review gate evidence — PASS

```text
Editor build: PASS

SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal: PASS
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable: PASS

Focused result: 4/4 PASS
```

The focused suite closes the previously missing R3 review evidence:

1. **Frozen status tooltip parity — PASS**
   - an identifiable frozen `FBattleHUDStatusView` is forwarded through the existing `RebuildTooltip` bridge;
   - StatusId, RuntimeSequence, display name and Amount are preserved;
   - inspect shows the optional tooltip and inspect-clear collapses it;
   - no formal `WB_PlayerStatuses` / `WB_EnemyStatuses` lifecycle behavior is touched.

2. **Terminal historical surface parity — PASS**
   - Native rendering is driven from ViewModel state only;
   - `Outcome=None` keeps the terminal surface collapsed and clears outcome text;
   - `Victory`, `Defeat`, and `ResolutionFaulted` render `胜利`, `战斗失败`, and `战斗结算异常` respectively.

3. **PresentationUnavailable rendering parity — PASS**
   - the test enters the state through `UBattleHUDViewModel::EnterPresentationUnavailable`;
   - input is locked and EndTurn/Confirm/Cancel cannot submit input;
   - the supplied ViewModel failure reason is rendered on the feedback surface;
   - `Outcome` remains `None`, the terminal overlay remains collapsed, and the state is not rendered as `ResolutionFaulted`.

4. **Block badge parity — PASS**
   - Player and Enemy Block `0` collapse the complete shield badge;
   - positive frozen Block restores the badge with the exact number;
   - returning Block to `0` collapses the whole badge again;
   - no Damage or BlockChanged Record playback is required.

## Regression boundary

The review-fix diff remains limited to the Native HUD implementation, Editor-only focused tests, and this validation documentation. It does not modify production `L_BattleTest`, the Legacy WBP assets, Controller/Reducer/Record/Envelope behavior, Gameplay authority, or R4+ implementation.

The earlier R3-A acceptance already established the Native WBP compile / PIE and production Legacy boundary. This review-fix run adds the focused C++ build and 4/4 Automation evidence required to close the two review findings.

Expected incomplete Native behavior at this stage remains intentional:

```text
Hand remains unmigrated until R4.
Committed Damage number/animation remains unmigrated until R7.
Formal status-row lifecycle remains unmigrated until R9.
```

## Acceptance

**R3-A remains COMPLETE / VALIDATED after the review fixes.**

The review findings are closed. R4 remains NOT STARTED until explicitly begun.
