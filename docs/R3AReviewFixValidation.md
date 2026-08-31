# Phase 6UI-A2N — R3-A Review Fix Validation

Status: **IMPLEMENTED / VALIDATION PENDING**

Branch: `a2n/r3-review-fix`
Base: `main` at `41ca570edf820b418a900c8826d4c424538325d4`

This review fix closes the missing Native combatant-inspection tooltip behavior without entering R9 status-row lifecycle ownership.

## Code fix

`UBattleHUDWidget::HandleCombatantInspectRequested` now forwards the selected combatant's frozen `FBattleHUDCombatantView::Statuses` array to the existing optional Designer tooltip function:

```text
RebuildTooltip(TArray<FBattleHUDStatusView>)
```

The bridge uses only the frozen ViewModel DTO. It does not create, update, remove, identify, or reconcile formal status-row widgets. Those rules remain reserved for R9.

If the optional tooltip surface does not expose the expected `RebuildTooltip` function, the Native HUD fails closed for that optional surface and keeps it collapsed rather than showing stale tooltip content.

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

## Minimal regression boundary

Also confirm after those focused checks:

```text
Editor build PASS
WBP_BattleHUD_Native compile PASS
production L_BattleTest still uses WBP_BattleHUD_C
Legacy WBP_BattleHUD / WBP_BattleCard / WBP_BattleStatus unchanged
R4 and later still NOT STARTED
```

After all checks pass, update `docs/CODEX_GOAL_CHECKPOINT.md` and `docs/Validation.md` with the actual evidence, then merge this branch. Until then, this branch is explicitly **VALIDATION PENDING**.
