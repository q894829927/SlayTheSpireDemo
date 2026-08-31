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

## Focused Editor-only Automation

A permanent Editor-only focused suite now exercises the four R3 review contracts without requiring Hand/Card migration, real combat completion, Damage playback, or formal Status-row lifecycle:

```text
SlayTheSpireDemo.Phase6UIA2N.R3.BlockBadge
SlayTheSpireDemo.Phase6UIA2N.R3.StatusTooltip
SlayTheSpireDemo.Phase6UIA2N.R3.Terminal
SlayTheSpireDemo.Phase6UIA2N.R3.PresentationUnavailable
```

The test probes live only in `SlayTheSpireDemoTests` and production runtime does not depend on them.

Run the whole focused prefix with:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  "E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -ExecCmds="Automation RunTests SlayTheSpireDemo.Phase6UIA2N.R3; Quit" `
  -unattended -nopause `
  -testexit="Automation Test Queue Empty" `
  -log
```

This suite is newly added and has **not yet been executed on the user's UE5.8 machine**. Do not mark the gates PASS until the actual run reports all four tests successful.

## Review gates still required

The earlier R3-A evidence did not independently close all acceptance items from `docs/Phase6UIA2NNativeHUDRefactor.md`. Do not merge this branch or treat the review fix as validated until the following focused checks pass on the saved branch head:

1. **Frozen status tooltip parity**
   - the automation injects an identifiable frozen `FBattleHUDStatusView`;
   - confirm the tooltip bridge receives the exact frozen StatusId, RuntimeSequence, display name and Amount;
   - confirm inspect shows the optional tooltip and inspect-clear collapses it;
   - no formal `WB_PlayerStatuses` / `WB_EnemyStatuses` lifecycle behavior is touched.

2. **Terminal historical surface parity**
   - automation drives the Native HUD from ViewModel state only;
   - `Outcome=None` => terminal overlay collapsed and outcome text empty;
   - `Outcome=Victory` => terminal overlay visible with `胜利`;
   - `Outcome=Defeat` => terminal overlay visible with `战斗失败`;
   - `Outcome=ResolutionFaulted` => terminal overlay visible with `战斗结算异常`.

3. **PresentationUnavailable rendering parity**
   - automation enters PresentationUnavailable through `UBattleHUDViewModel::EnterPresentationUnavailable`;
   - confirm `bInputLocked=true`, `bCanEndTurn=false`, and EndTurn/Confirm/Cancel are disabled or collapsed;
   - confirm `Txt_Feedback` shows the supplied ViewModel failure reason;
   - confirm `Outcome` remains `None`, terminal overlay stays collapsed, and no ResolutionFault terminal text is rendered.

4. **Block badge parity**
   - automation starts Player and Enemy at frozen Block `0` and confirms both complete badge surfaces collapse;
   - frozen positive Block shows the badge with the exact value;
   - returning Block to `0` collapses the badge again;
   - no Damage or BlockChanged Record playback is required.

## Minimal regression boundary

After the focused Automation passes, also confirm:

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
