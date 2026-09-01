# Phase 6UI-A2N — R14-A Safe Cleanup Validation

Date: **2026-09-01**

Status: **R14-A IN PROGRESS; R14-B NOT AUTHORIZED**

## Scope

R14-A is limited to non-destructive cleanup after the validated Native production cutover and stabilization milestone.

Allowed cleanup:

```text
migration-only compatibility code
confirmed-unreferenced helpers
abandoned Native-stack code
durable documentation updates
```

Not allowed in R14-A:

```text
Legacy WBP deletion or rename
redirector cleanup associated with Legacy deletion
Gameplay/Presentation semantic redesign
UI-A3 work
```

Legacy assets remain intact:

```text
WBP_BattleHUD
WBP_BattleCard
WBP_BattleStatus
```

## Starting point

```text
Starting HEAD: e1b60480807ae1a140acc637a5873990d2937722
Native HUD: production default
R0-R13: COMPLETE / VALIDATED
R14-B: NOT AUTHORIZED
```

## R14-A1 — Confirmed-unreferenced C++ helpers

Inventory found two protected helper accessors with no production or test call sites:

```text
UBattleHUDWidget::AreNativeBindingsValid()
UBattleCardWidget::AreNativeBindingsValid()
```

Removed in two isolated source commits:

```text
5e5dded7f28780a036d71ff91db0fcd45da18071
refactor(ui-a2n): remove unused native card binding helper

d8674d91588c3d7b98c964647d22f80218841189
refactor(ui-a2n): remove unused native HUD binding helper
```

The underlying `bNativeBindingsValid` state remains unchanged and continues to be used internally. No reflected property/function, Gameplay contract, Presentation contract, Controller behavior, WidgetClass, WBP asset, or Legacy asset was changed.

### Automated gates

User-confirmed on **2026-09-01**:

```text
Editor Build: PASS
SlayTheSpireDemo.Phase6UIA2N.R3 focused Automation: PASS
SlayTheSpireDemo.Phase6UIA2N.R4 focused Automation: PASS
Reference/source scan: helper definitions removed; no known production/test call sites
```

No exact Automation discovery count is claimed here because the user reported the gate result as PASS rather than an exact count.

### Manual PIE gate

Production map:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

User-confirmed on **2026-09-01**:

```text
Native HUD opens normally
Hand is visible
one ordinary card play resolves and displays normally
input returns after catch-up
no binding/runtime error observed
```

Result: **PASS**

### R14-A1 result

**R14-A1 COMPLETE / VALIDATED.**

## R14-A2 — Native Blueprint migration residue audit

The migration documents and source establish these Native-duplicate residues:

```text
WBP_BattleCard_Native
- CardView

WBP_BattleStatus_Native
- StatusView
- CurrentStatusView
- MID_StatusIcon
```

`UBattleCardWidget` owns the active frozen Card DTO in native `CurrentCardView`, and the R4 validation record explicitly describes the duplicated Blueprint `CardView` as inert migration residue. `UBattleStatusWidget` owns the active frozen Status DTO/material state in `NativeStatusView` / `NativeStatusIconMID`; the R9 validation record explicitly describes the three duplicated Blueprint variables as retained migration residue.

The Native duplicate business graphs were removed during migration (`EventGraph nodes=0` in the recorded Native asset inspection), so these variables do not own the migrated runtime behavior.

The HUD duplicate also inherited the Legacy presentation-local Blueprint variable set. The Legacy saved snapshot records the presentation locals used by the old business graphs, including:

```text
ActivePresentationToken
ActivePresentationType
ActivePresentationTimer
PlayedCardWidget
HiddenHandCardWidget
ZoneChangedDrawnCardWidget
ActiveStatusPresentationWidget
bDamageTargetIsPlayer
bBlockTargetIsPlayer
```

R13 independently confirmed that four transient Widget-reference variables still existed in `WBP_BattleHUD_Native` and changed their concrete types from Legacy Card/Status classes to Native Card/Status classes solely to remove production Legacy package dependencies. Because the Native HUD business graph is empty and C++ owns the active Token/timer/card/status presentation state, these inherited HUD variables are cleanup candidates as well.

### Asset-level confirmation boundary

The repository stores the `.uasset` files as Git LFS pointer entries, so the GitHub-side audit cannot inspect the current serialized Blueprint variable-reference graph directly. R14-A therefore requires an Editor-side `Find References` / Blueprint-variable inspection before deleting any of the above variables.

Required asset check:

```text
WBP_BattleCard_Native
- CardView: Find References -> zero executable/property-binding uses

WBP_BattleStatus_Native
- StatusView: Find References -> zero executable/property-binding uses
- CurrentStatusView: Find References -> zero executable/property-binding uses
- MID_StatusIcon: Find References -> zero executable/property-binding uses

WBP_BattleHUD_Native
- inspect the nine inherited presentation-local variables listed above
- delete only variables whose Find References result is zero and which are not required by Designer/property bindings
```

Do not delete any Designer `BindWidget` variable or any Legacy WBP variable.

Status: **USER ACTION REQUIRED FOR ASSET-LEVEL CONFIRMATION / EDIT**

### R14-A2 validation after asset edit

If the Editor-side reference check is zero and the residue is deleted from Native duplicates only:

```text
1. Compile / Save / close / reopen affected Native WBP assets
2. Editor Build
3. SlayTheSpireDemo.Phase6UIA2N.R4 focused Automation
4. SlayTheSpireDemo.Phase6UIA2N.R9 focused Automation
5. one production /Game/SlayTheSpireDemo/Maps/L_BattleTest PIE smoke
```

No Phase6R, Shipping or Scenario A-E rerun is required for this non-destructive cleanup slice unless a concrete failure invalidates a broader Gate.

## Deferred cleanup inventory

The following candidate remains outside R14-A2 until a separate asset-reference decision:

```text
L_BattleTest_Native
```

Shared Legacy compatibility surfaces and permanent Automation seams remain retained unless a later R14-A slice proves they are genuinely unreferenced.

## Current phase state

```text
R0-R13 COMPLETE / VALIDATED
R14-A IN PROGRESS
R14-A1 COMPLETE / VALIDATED
R14-A2 USER ACTION REQUIRED
R14-B NOT AUTHORIZED
Legacy assets retained
UI-A3 NOT STARTED
```
