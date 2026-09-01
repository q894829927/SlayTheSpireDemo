# Phase 6UI-A2N — R13 Native HUD Stabilization

Status:

```text
R0-R12 COMPLETE / VALIDATED
R13 STABILIZATION IN PROGRESS
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
[x] At least one real post-cutover Native-only UI implementation change exists
[x] That change has focused validation PASS
[x] Objective R13 stabilization tests PASS
[ ] Scenario A-E final production PIE PASS
[ ] active Skip final production PIE PASS
[ ] active Cancel final production PIE PASS
[ ] formal current-head Phase6R PASS
[ ] clean-worktree Shipping exclusion PASS
[x] production runtime Legacy HUD/Card/Status dependency = 0
```

The first condition is still being tracked and is not checked until milestone
closure. R12 evidence is a prerequisite, not a substitute for the R13 final-head
Gates.

## Post-cutover Native-only UI change

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

The opening audit correctly found no qualifying implementation change at the R13
starting HEAD. During R13-M1, the production dependency audit then identified a real
stabilization defect: the Presenter Blueprint default still selected the Legacy HUD,
and four Native HUD transient Blueprint variables retained Legacy concrete Card or
Status types. The production map instance override hid the Presenter-default defect,
while the variable types kept Legacy Card/Status packages in the formal Native
runtime dependency closure.

The authorized Native-only stabilization change is:

```text
Qualifying post-cutover Native-only UI implementation change: YES
Change commit: fe7fe4e
Subject: fix(ui-a2n): remove production legacy widget dependencies
Purpose: make the production Presenter default Native and remove concrete Legacy
         Card/Status types from Native HUD transient presentation ownership
```

Changed files:

```text
Content/SlayTheSpireDemo/Blueprints/Battle/BP_BattleHUDPresenter.uasset
Content/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.uasset
Source/SlayTheSpireDemoTests/SlayTheSpireDemoTests.Build.cs
Source/SlayTheSpireDemoTests/Private/Phase6UIA2NR13Tests.cpp
```

`WBP_BattleHUD_Native` changed three Card transient variables to
`WBP_BattleCard_Native_C` and one Status transient variable to
`WBP_BattleStatus_Native_C`. The formal `CardWidgetClass` and `StatusWidgetClass`
defaults remain Native. No Legacy asset was changed or dual-written.

Focused validation on the change:

```text
SlayTheSpireDemoEditor Win64 Development: PASS
SlayTheSpireDemo.Phase6UIA2N.R13 discovery: exactly 1
SlayTheSpireDemo.Phase6UIA2N.R13: 1/1 Success
failed: 0
notRun: 0
```

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

The actual R13 change invalidated the asset boundary, not the sealed playback
semantics. One permanent Editor-only objective test was therefore added at the
affected boundary:

```text
SlayTheSpireDemo.Phase6UIA2N.R13.AssetReferences.NativeProductionClosure
```

It loads the production map and Blueprint defaults, verifies the Native HUD/Card/
Status classes, and traverses hard package dependencies from the production map.
The exact focused result is 1/1 Success, 0 failed, 0 notRun. A duplicate synthetic
Skip/Cancel suite was not added: the real-producer R11/R12 evidence remains sealed,
and R13 still requires final production-map active Skip and active Cancel PIE.

## Asset-reference audit — PASS

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

The initial R13 audit found these three hard dependencies:

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

After `fe7fe4e`, a fresh UE process reopened the assets and repeated the same loaded-
property and Asset Registry inspection. Formal result:

```text
Production instance WidgetClass: WBP_BattleHUD_Native_C
BP_BattleHUDPresenter CDO WidgetClass: WBP_BattleHUD_Native_C
Native HUD CardWidgetClass: WBP_BattleCard_Native_C
Native HUD StatusWidgetClass: WBP_BattleStatus_Native_C
Native HUD direct Legacy dependency count: 0
Production runtime Legacy HUD/Card/Status dependency: 0
Required result: 0
```

Legacy assets were not deleted, renamed or modified. Their SHA-256 values remain:

```text
WBP_BattleHUD    990125C951D52D5F23194D9EB7C079C2F3C514C78A285DF0DDE273B6B1C0F94A
WBP_BattleCard   1E7579EAFE8BF49AEB953B521604CDE4C442E6580BDEB3E071C210846BC6631F
WBP_BattleStatus 205180C8DF03DAE5D825AB4428ADD4B90EDFBBBB54F9BFEFE76AF07412DA52D2
```

## R13 final automated gates

Current evidence:

```text
A. SlayTheSpireDemoEditor Win64 Development: PASS
B. focused SlayTheSpireDemo.Phase6UIA2N.R13: exactly 1/1 PASS
C. formal current-head Phase6R: NOT RUN
D. clean-worktree SlayTheSpireDemo Win64 Shipping exclusion: NOT RUN
E. production runtime Legacy HUD/Card/Status dependency: 0 — PASS
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

## Remaining closure gates and stop boundary

The two opening blockers are closed by the real Native-only change and dependency
audit above. R13 remains `STABILIZATION IN PROGRESS` until the formal current-head
Phase6R and clean Shipping Gates pass and the user completes the single final
production-map Scenario A-E plus active Skip/Cancel PIE pass. R14-A, R14-B, Legacy
deletion/renaming, redirector cleanup and UI-A3 remain not started and unauthorized.
