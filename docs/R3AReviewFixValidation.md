# Phase 6UI-A2N — R3-A Review Fix Validation

Status: **IMPLEMENTED / VALIDATION PENDING**

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

## Review gates still required

The earlier R3-A evidence did not independently close all acceptance items from `docs/Phase6UIA2NNativeHUDRefactor.md`. Do not merge this branch or treat the review fix as validated until the following focused checks pass on the saved branch head:

1. **Frozen status tooltip parity**
   - use a combatant whose frozen `CombatantView.Statuses` contains at least one identifiable status;
   - hover/focus the corresponding combatant presentation;
   - confirm the tooltip is rebuilt from that frozen array and displays the expected status content;
   - clear hover/focus and confirm the tooltip collapses;
   - no formal `WB_PlayerStatuses` / `WB_EnemyStatuses` lifecycle behavior is changed by this check.

2. **Terminal historical surface parity**
   - drive the Native HUD from frozen/ViewModel state only;
   - `Outcome=None` => terminal overlay collapsed and outcome text empty;
   - `Outcome=Victory` => terminal overlay visible with `胜利`;
   - `Outcome=Defeat` => terminal overlay visible with `战斗失败`;
   - `Outcome=ResolutionFaulted` => terminal overlay visible with `战斗结算异常`;
   - do not read mutable Gameplay to construct these surfaces.

3. **PresentationUnavailable rendering parity**
   - enter `InteractionState=PresentationUnavailable` through the existing ViewModel boundary;
   - confirm `bInputLocked=true` and EndTurn/Confirm/Cancel cannot submit input;
   - confirm `Txt_Feedback` shows the ViewModel `LastFeedback` reason;
   - confirm `Outcome` remains `None` and the terminal overlay remains collapsed;
   - confirm the state is not rendered or reported as `ResolutionFaulted`.

4. **Block badge parity**
   - start with Player and Enemy frozen Block equal to `0` and confirm both shield badges are collapsed (no shield icon and no `0` text);
   - apply a frozen/ViewModel state with Block `> 0` and confirm the corresponding shield badge becomes visible with the exact Block value;
   - return that combatant to Block `0` and confirm the whole badge collapses again;
   - this check is static ViewModel rendering only and must not require Damage or BlockChanged Record playback.

## Minimal regression boundary

Also confirm after those focused checks:

```text
Editor build PASS
WBP_BattleHUD_Native compile PASS
production L_BattleTest still uses WBP_BattleHUD_C
Legacy WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus unchanged
R4 and later still NOT STARTED
```

Expected incomplete Native behavior at this stage is not a failure:

```text
Hand remains unmigrated until R4.
Committed Damage number/animation remains unmigrated until R7.
Formal status-row lifecycle remains unmigrated until R9.
```

After all checks pass, update `docs/CODEX_GOAL_CHECKPOINT.md` and `docs/Validation.md` with the actual evidence, then merge this branch. Until then, this branch is explicitly **VALIDATION PENDING**.
