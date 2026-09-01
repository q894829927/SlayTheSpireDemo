# Phase 6UI-A2N — R12 Production Cutover

Status:

```text
R0-R10 COMPLETE / VALIDATED
R11 VALIDATION INCOMPLETE
R12-A AUTHORIZED EARLY BY EXPLICIT USER OVERRIDE
R12-A CUTOVER PENDING LOCAL UE MAP EDIT
R12-B NOT VALIDATED
R13 NOT STARTED
```

Date: **2026-09-01**
Branch: `main`

## Explicit ordering override

The user explicitly requested that R12 be performed before the remaining R11 validation checks are finished. This overrides the normal phase order for execution only. It does **not** make the incomplete R11 gates pass and it does **not** authorize any R12-B validation claim before those gates are actually run.

The remaining R11 checks are still pending unless separately confirmed:

```text
active Skip
active Cancel
stale callback rejection
Input Unlock after catch-up
remaining automated candidate gates, if not yet confirmed
```

## R12-A scope

The only permitted production change is the unique `ABattleHUDPresenter::WidgetClass` instance on:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
```

Change:

```text
WBP_BattleHUD_C
-> WBP_BattleHUD_Native_C
```

No Gameplay, Presentation, Controller/reducer, Record/Envelope schema, Native implementation, Legacy asset, cleanup, deletion, or UI-A3 change belongs in the R12-A cutover commit.

## Current execution constraint

The production `WidgetClass` is stored in the binary Unreal map asset `Content/SlayTheSpireDemo/Maps/L_BattleTest.umap`. The connected GitHub contents API cannot safely rewrite that binary UE package. Therefore no R12-A PASS or cutover commit is claimed here.

The local UE editor must perform and save the one property edit. After that save, the expected repository diff for the isolated R12-A cutover is:

```text
Content/SlayTheSpireDemo/Maps/L_BattleTest.umap
```

and no unrelated runtime/source/assets.

Recommended isolated commit subject:

```text
feat(ui-a2n): cut production HUD over to native
```

## R12-B

R12-B remains NOT VALIDATED. Its formal acceptance must run from the production cutover head and cannot be inferred from R11 Native test-map evidence.

Do not mark R12 COMPLETE / VALIDATED until the required cutover-head automated and manual gates have actually passed.
