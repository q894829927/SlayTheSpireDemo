# Legacy UI Preservation Policy

Date: **2026-09-01**

Status: **ACTIVE / DURABLE**

## Decision

The former Legacy battle UI assets are intentionally retained for historical reference and emergency recovery, but they are permanently deprecated for normal development.

Retained Legacy assets:

```text
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleHUD
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleCard
/Game/SlayTheSpireDemo/UI/Out/Legacy/WBP_BattleStatus
```

On **2026-09-01**, Unreal AssetTools relocated these retained deprecated assets from
`/Game/SlayTheSpireDemo/UI/Widgets/` to `/Game/SlayTheSpireDemo/UI/Out/Legacy/`.
Their asset names and Legacy business implementation remain unchanged. The
relocation is not R14-B and does not authorize deletion or runtime reactivation.

They remain:

```text
RETAINED
DEPRECATED
DO NOT USE
```

They are not part of the active runtime UI stack.

The active battle UI stack is:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
→ BP_BattleHUDPresenter
→ WBP_BattleHUD_Native
→ WBP_BattleCard_Native
→ WBP_BattleStatus_Native
```

Production runtime Legacy HUD/Card/Status dependency count must remain:

```text
0
```

## Prohibited Normal Use

Unless the user explicitly authorizes a recovery or Legacy-removal task, do not use the retained Legacy assets in any new or modified runtime path.

Specifically, do not:

```text
set a production or test Presenter WidgetClass to WBP_BattleHUD
set Native HUD CardWidgetClass to WBP_BattleCard
set Native HUD StatusWidgetClass to WBP_BattleStatus
add new Blueprint or C++ hard references to any of the three Legacy WBP assets
implement new UI behavior in the Legacy stack
dual-write a UI change into both Native and Legacy stacks
use Legacy assets as the execution target for new regression tests
restore Legacy fallback merely because the assets still exist
copy new Native behavior back into Legacy for parity maintenance
```

Historical documentation may continue to mention the Legacy assets as historical evidence. Reading or opening the assets for inspection is allowed and is not runtime fallback.

## Allowed Uses

The retained assets may be used only for:

```text
historical/reference inspection
migration archaeology
explicitly authorized emergency recovery
```

Emergency recovery must be a new explicit user decision. The existence of these assets alone is never authorization to restore them to production or tests.

## Native-Only Forward Development

All new battle-HUD implementation work must target the Native stack only:

```text
UBattleHUDWidget / WBP_BattleHUD_Native
UBattleCardWidget / WBP_BattleCard_Native
UBattleStatusWidget / WBP_BattleStatus_Native
```

Gameplay, Presentation, ViewModel and Controller authority remain governed by their existing contracts. This policy changes only which UI implementation stack is allowed for forward development.

## R14-B Status

R14-B destructive Legacy removal is **NOT REQUIRED under the current project decision and remains NOT AUTHORIZED**. The archive relocation is not R14-B.

Do not delete, rename or move the retained Legacy assets, and do not Fix Redirectors for their removal, unless the user later gives a separate explicit authorization.

If R14-B is ever explicitly authorized, follow the destructive-removal gates in `docs/Phase6UIA2NNativeHUDRefactor.md` before deleting anything.

## Regression Rule

Any future change that causes the production or Native UI dependency graph to reference one of the retained Legacy HUD/Card/Status assets is a regression unless it is part of an explicitly authorized recovery task.

When a change affects UI asset dependencies, preserve or re-run the smallest applicable dependency gate that proves:

```text
Production runtime Legacy HUD/Card/Status dependency count = 0
```
