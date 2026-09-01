# Phase 6UI-A2N — R14-A Safe Cleanup Validation

Date: **2026-09-01**

Status: **R14-A COMPLETE / VALIDATED; R14-B NOT AUTHORIZED**

## Scope

R14-A is limited to non-destructive cleanup after the validated Native production
cutover and stabilization milestone. Legacy HUD/Card/Status assets remain retained
and unchanged. R14-A does not authorize Legacy deletion, redirector cleanup,
Gameplay/Presentation redesign, production WidgetClass changes, or UI-A3 work.

## Starting point

```text
R14-A starting HEAD: e1b60480807ae1a140acc637a5873990d2937722
R14-A2 starting HEAD: 2ee470e
Native HUD: production default
R0-R13: COMPLETE / VALIDATED
R14-B: NOT AUTHORIZED
```

## R14-A1 — Confirmed-unreferenced C++ helpers

Removed in isolated commits:

```text
5e5dded7f28780a036d71ff91db0fcd45da18071
UBattleCardWidget::AreNativeBindingsValid()

d8674d91588c3d7b98c964647d22f80218841189
UBattleHUDWidget::AreNativeBindingsValid()
```

The internal `bNativeBindingsValid` state remains unchanged. R14-A1 Editor Build,
R3 focused Automation, R4 focused Automation, and the production `L_BattleTest` PIE
smoke passed. R14-A1 is **COMPLETE / VALIDATED**.

## R14-A2 — Native Blueprint migration residue

### Asset-level reference audit

Each candidate was audited against the current saved Blueprint asset using UE Editor
Blueprint graph/member inspection and full UE object text export. The audit checked
EventGraph, functions, macros, serialized property bindings, animations, and
Designer/default dependencies. All three Native assets contained only an empty
`EventGraph`; serialized property-binding and animation entries were both zero.
Every candidate appeared only as its member-variable declaration and had zero
executable, binding, or required Designer references.

| Asset | Variable | References | Result | Reason |
|---|---|---:|---|---|
| `WBP_BattleCard_Native` | `CardView` | 0 | DELETED | C++ `CurrentCardView` owns the formal DTO. |
| `WBP_BattleStatus_Native` | `StatusView` | 0 | DELETED | C++ `NativeStatusView` owns the formal state. |
| `WBP_BattleStatus_Native` | `CurrentStatusView` | 0 | DELETED | C++ `NativeStatusView` owns the formal state. |
| `WBP_BattleStatus_Native` | `MID_StatusIcon` | 0 | DELETED | C++ `NativeStatusIconMID` owns the formal MID. |
| `WBP_BattleHUD_Native` | `ActivePresentationToken` | 0 | DELETED | Native playback kernel owns the token. |
| `WBP_BattleHUD_Native` | `ActivePresentationType` | 0 | DELETED | Native playback kernel owns the type. |
| `WBP_BattleHUD_Native` | `ActivePresentationTimer` | 0 | DELETED | Native playback kernel owns the timer. |
| `WBP_BattleHUD_Native` | `PlayedCardWidget` | 0 | DELETED | Native Card lifecycle owns its transient. |
| `WBP_BattleHUD_Native` | `HiddenHandCardWidget` | 0 | DELETED | Native Card lifecycle owns its transient. |
| `WBP_BattleHUD_Native` | `ZoneChangedDrawnCardWidget` | 0 | DELETED | Native Card lifecycle owns its transient. |
| `WBP_BattleHUD_Native` | `ActiveStatusPresentationWidget` | 0 | DELETED | Native Status lifecycle owns its transient. |
| `WBP_BattleHUD_Native` | `bDamageTargetIsPlayer` | 0 | DELETED | Native Damage presentation owns target state. |
| `WBP_BattleHUD_Native` | `bBlockTargetIsPlayer` | 0 | DELETED | Native Block presentation owns target state. |

`CardWidgetClass`, `StatusWidgetClass`, all Designer Widgets, and all C++
`BindWidget`/`BindWidgetOptional` surfaces were retained unchanged. No Legacy asset
was edited; the three Legacy SHA-256 hashes remain the sealed R13 values.

### Native Blueprint compile/save/reopen

Assets were processed one at a time. Each asset compiled and saved immediately after
its deletions, then was loaded and compiled again in a fresh UE Editor process.

```text
WBP_BattleCard_Native:   PASS, BS_UP_TO_DATE, CardView absent after reopen
WBP_BattleStatus_Native: PASS, BS_UP_TO_DATE, all three candidates absent after reopen
WBP_BattleHUD_Native:    PASS, BS_UP_TO_DATE, all nine candidates absent after reopen
Blueprint compile errors: 0
```

### Automated gates

```text
SlayTheSpireDemoEditor Win64 Development: PASS (Result: Succeeded)

SlayTheSpireDemo.Phase6UIA2N.R4:
  exactly 1/1 Success, 0 warnings, 0 failed, 0 notRun

SlayTheSpireDemo.Phase6UIA2N.R9:
  exactly 5/5 Success, 0 warnings, 0 failed, 0 notRun

SlayTheSpireDemo.Phase6UIA2N.R13.AssetReferences.NativeProductionClosure:
  exactly 1/1 Success, 0 warnings, 0 failed, 0 notRun
```

The R13 Asset Registry/loaded-property Gate reconfirmed:

```text
L_BattleTest Presenter WidgetClass = WBP_BattleHUD_Native_C
WBP_BattleHUD_Native CardWidgetClass = WBP_BattleCard_Native_C
WBP_BattleHUD_Native StatusWidgetClass = WBP_BattleStatus_Native_C
Production runtime Legacy HUD/Card/Status dependency count = 0
Native HUD direct Legacy Card/Status dependency count = 0
```

No Phase6R, A2D5, Shipping, Scenario A-E, parity, broad historical suite, or
architecture review was run for R14-A2.

### Manual PIE gate

The user completed the production-map smoke on **2026-09-01**:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

Required observation:

```text
Native HUD creates normally
Hand, Energy, HP, and pile counts display normally
one ordinary attack card has normal Card and Damage presentation
the Card reaches its correct final zone
input returns after catch-up
no duplicate or A -> B -> A flashback
no Native binding or Blueprint runtime error in Output Log
```

Result: **PASS**

The user confirmed normal Native HUD creation; correct Hand, Energy, HP, and pile
surfaces; normal Card and Damage presentation for one ordinary attack; the correct
final Card zone; input recovery after catch-up; and no duplicate, flashback, Native
binding error, or Blueprint runtime error.

R14-A2 is **COMPLETE / VALIDATED**. The implementation commit is:

```text
8a609659ba138c922fe64bbfd08bca44b05ca8d6
refactor(ui-a2n): remove native blueprint migration residue
```

## L_BattleTest_Native retention decision

`L_BattleTest_Native` is intentionally retained as a non-production migration and
regression map. Production already uses `L_BattleTest` with
`WBP_BattleHUD_Native_C`, and the current production runtime Legacy HUD/Card/Status
dependency count is zero.

The remaining references to `L_BattleTest_Native` are primarily historical A2N
validation/checkpoint documentation. Deleting the map provides no concrete current
maintenance or packaging benefit and would introduce a separate asset-deletion,
reference-audit, and smoke-validation slice. Therefore its retention is an explicit
R14-A decision, not an unfinished cleanup blocker.

No redirector cleanup is required because the map was not renamed or deleted.

## R14-A closure

R14-A is **COMPLETE / VALIDATED**.

Completed safe-cleanup slices:

```text
R14-A1 confirmed-unreferenced C++ helper cleanup: COMPLETE / VALIDATED
R14-A2 Native Blueprint migration-residue cleanup: COMPLETE / VALIDATED
L_BattleTest_Native retention decision: RETAINED INTENTIONALLY
```

R14-A changed no Gameplay authority, Presentation Record/Envelope semantics,
Controller/reducer ownership, production WidgetClass, or Legacy WBP asset. Legacy
HUD/Card/Status assets remain available for the separately authorized R14-B boundary.

No additional Build, Automation, Blueprint compile, or PIE run is claimed for this
pure documentation seal; the implementation/asset evidence above remains the
accepted R14-A evidence.

## Final phase state

```text
R0-R13 COMPLETE / VALIDATED
R14-A COMPLETE / VALIDATED
R14-B NOT AUTHORIZED
Native HUD = production default
Production runtime Legacy HUD/Card/Status dependency count = 0
Legacy assets retained
L_BattleTest_Native retained intentionally as non-production migration/regression map
UI-A3 NOT STARTED
```

R14-B remains destructive and may begin only after a separate explicit user
authorization. Until then, do not delete or rename `WBP_BattleHUD`,
`WBP_BattleCard`, or `WBP_BattleStatus`, and do not run deletion-related Fix
Redirectors.
