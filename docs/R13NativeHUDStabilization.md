# Phase 6UI-A2N — R13 Native HUD Stabilization

Status:

```text
R0-R12 COMPLETE / VALIDATED
R13 STABILIZATION IN PROGRESS
BLOCKED ON REAL POST-CUTOVER NATIVE-ONLY UI CHANGE
BLOCKED ON PRODUCTION LEGACY RUNTIME DEPENDENCY AUDIT
R14 NOT STARTED
```

Date opened: **2026-09-01**
Starting HEAD: `76d411a21c042a86d1e7a4c608a67ae10c724ea2`

## R13-M1 — Native Production Stabilization

R13-M1 is a finite objective milestone. It closes only when every listed completion
condition has current evidence; elapsed time or an informally observed “stable
cycle” is not evidence.

Formal production configuration at milestone start:

```text
Map:
/Game/SlayTheSpireDemo/Maps/L_BattleTest

WidgetClass:
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C

R12-A cutover commit:
de788c5b68e06827f8fdba3b83858f86a385bdeb

R12 final seal commit / R13 starting HEAD:
76d411a21c042a86d1e7a4c608a67ae10c724ea2
```

R12 seal evidence remains recorded in
`docs/R12NativeProductionCutoverValidation.md`: Native WBP 3/3, A2D5 6/6,
formal Phase6R 100/100, clean Shipping, Scenario A-E, active Skip/Cancel, stale
callback and Input Unlock all passed on the production Native configuration.

Legacy fallback inventory remains present and unchanged:

```text
WBP_BattleHUD
WBP_BattleCard
WBP_BattleStatus
```

Legacy runtime fallback occurred: **NO**

Opening Legacy assets or inspecting their references is not counted as fallback.
Production `WidgetClass` was not restored to Legacy and no production PIE used the
Legacy stack during this milestone.

## Objective completion conditions

R13-M1 remains open until all conditions are true:

```text
[ ] Native remained production default throughout R13-M1
[x] Legacy runtime fallback occurred = NO
[ ] At least one real post-cutover Native-only UI implementation change exists
[ ] That change has focused validation PASS
[ ] Objective R13 stabilization tests PASS
[ ] Scenario A-E final production PIE PASS
[ ] active Skip final production PIE PASS
[ ] active Cancel final production PIE PASS
[ ] formal current-head Phase6R PASS
[ ] clean-worktree Shipping exclusion PASS
[ ] production runtime Legacy HUD/Card/Status dependency = 0
```

The first condition is still being tracked and is not checked until milestone
closure. R12 evidence is a prerequisite, not a substitute for the R13 final-head
Gates.

## Post-cutover Native-only UI change audit

Git history from the cutover commit through the R13 starting HEAD contains only
documentation work after `de788c5`:

```text
008c734 docs(ui-a2n): open R12 early-cutover checkpoint
2fc9f77 merge containing only the R12 checkpoint document
76d411a docs(ui-a2n): seal R12 production cutover
```

Changed paths are limited to:

```text
docs/CODEX_GOAL_CHECKPOINT.md
docs/DevelopmentPhases.md
docs/Phase6UIA2NNativeHUDRefactor.md
docs/R12NativeProductionCutoverValidation.md
docs/Validation.md
```

Result:

```text
Qualifying post-cutover Native-only UI implementation change: NO
Change commit: none
Purpose: none
Changed Native implementation/assets: none
Focused validation: not applicable
```

Documentation, temporary validation harnesses, scripts, cleanup and comments do not
qualify. No cosmetic/no-op UI change will be created merely to close R13.

## Existing objective coverage audit

The current permanent suites already cover these stabilization boundaries:

| Boundary | Existing evidence | Assessment |
|---|---|---|
| exact Token ownership, Finish/Cancel, stale/duplicate Finish, destruction | R5 focused tests | covered |
| Energy, Block, Shuffle Before/After/Cancel and cleanup | R6 focused tests | covered |
| Player/Enemy Damage, Block absorption, lethal, Cancel, stale/destruct | R7 focused tests | covered |
| Card identity, sequential Draw, zone destinations, historical restore, retained-card Skip cleanup | R8 focused tests | covered |
| exact Status identity, reduction/removal/reapply, stale/destruct | R9 focused tests | covered |
| terminal ordering, Cancel restore, PresentationUnavailable separation | R10 focused tests | covered |
| timeout/Cancel Controller behavior | A2D4 | covered |
| final history, Card/Status integration and full turn-cycle ordering | A2D5 | covered |
| real Skip, timeout Cancel, stale callback and post-catch-up input | R11/R12 temporary harness evidence | validated, but not a permanent combined R13 regression |

No duplicate Status/Card/terminal test is justified. Two concrete R13 objective
gaps are reserved for the future candidate that contains a real Native-only change:

1. one deterministic combined sequence proving `active Skip -> later timeout Cancel
   -> stale callback -> later real request` without generation/transient leakage;
2. one Editor-only asset-contract test proving the production map, Native class
   defaults and Asset Registry dependency closure contain no Legacy HUD packages.

No R13 test source has been added yet. The asset-contract test cannot legitimately
pass while the dependency audit below is failing. Final expected R13 focused count
will be fixed only when the smallest justified tests are implemented.

## Asset-reference audit — FAIL

The audit used UE 5.8 Asset Registry dependency inspection, loaded production-map
properties and class defaults, plus a Runtime C++/config source scan. It did not use
binary grep as asset evidence.

Checks that passed:

```text
Production map Presenter instances: exactly 1
Production instance WidgetClass: WBP_BattleHUD_Native_C
Native HUD CardWidgetClass: WBP_BattleCard_Native_C
Native HUD StatusWidgetClass: WBP_BattleStatus_Native_C
Runtime C++ hard-coded Legacy WBP paths: 0
```

The formal production dependency closure nevertheless reaches all three Legacy
packages:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
-> /Game/SlayTheSpireDemo/Blueprints/Battle/BP_BattleHUDPresenter
-> /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD

/Game/SlayTheSpireDemo/Maps/L_BattleTest
-> /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native
-> /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleCard

/Game/SlayTheSpireDemo/Maps/L_BattleTest
-> /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native
-> /Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleStatus
```

All three owner/target pairs were reported as direct `HardPackage` dependencies.
The Presenter class default was independently loaded and confirmed as:

```text
BP_BattleHUDPresenter CDO WidgetClass = WBP_BattleHUD_C
```

The Native HUD package still reports direct Legacy Card/Status dependencies even
though its exposed class defaults resolve to the Native classes. The exact serialized
reference owner is therefore `WBP_BattleHUD_Native`; the cause has not been modified
or guessed beyond the Asset Registry evidence.

Formal result:

```text
Production runtime Legacy HUD dependency: 3
Required result: 0
```

No Legacy asset was deleted, renamed or modified. No production or Native asset was
changed in response to this failed audit.

## R13 final automated gates

These are defined but intentionally **NOT RUN** while prerequisite milestone
conditions are false:

```text
A. SlayTheSpireDemoEditor Win64 Development
B. focused SlayTheSpireDemo.Phase6UIA2N.R13 objective tests
C. formal current-head Phase6R workflow with exact current suite counts
D. clean-worktree SlayTheSpireDemo Win64 Shipping exclusion
E. production runtime Legacy HUD/Card/Status dependency = 0
```

## R13 final manual PIE gates

These are defined but intentionally **NOT RUN** before a valid R13 final candidate:

```text
Production L_BattleTest Scenario A-E
active Skip
active Cancel
```

The observable contract remains the frozen R12 contract; R13 adds no new visual
acceptance criteria.

## Current blockers and stop boundary

R13 cannot be marked `COMPLETE / VALIDATED` because:

1. no real post-cutover Native-only UI implementation change exists; and
2. the formal production asset dependency audit reports three Legacy HUD-stack
   dependencies instead of zero.

R13 remains `STABILIZATION IN PROGRESS`. R14-A, R14-B, Legacy deletion/renaming,
redirector cleanup and UI-A3 remain not started and unauthorized.
