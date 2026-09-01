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

```text
Editor Build: PENDING
Focused Native Automation: PENDING
Reference/source scan: helper definitions removed; GitHub code-search index may lag current HEAD
```

### Manual PIE gate

One production-map smoke is required after the source slice:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

Required observation:

```text
Native HUD opens normally
Hand is visible
one ordinary card play resolves and displays normally
input returns after catch-up
no binding/runtime errors
```

Result: **PENDING**

## Deferred cleanup inventory

The following candidates are not part of R14-A1 and must not be removed without an asset-level reference/Blueprint-variable audit:

```text
WBP_BattleCard_Native retained Legacy CardView variable
WBP_BattleStatus_Native retained Legacy StatusView / CurrentStatusView / MID_StatusIcon variables
other duplicated Native-WBP migration residue
L_BattleTest_Native
```

Shared Legacy compatibility surfaces and permanent Automation seams remain retained unless a later R14-A slice proves they are genuinely unreferenced.

## Current phase state

```text
R0-R13 COMPLETE / VALIDATED
R14-A IN PROGRESS
R14-A1 implementation complete / validation pending
R14-B NOT AUTHORIZED
Legacy assets retained
UI-A3 NOT STARTED
```
