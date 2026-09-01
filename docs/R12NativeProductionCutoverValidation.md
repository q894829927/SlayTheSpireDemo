# Phase 6UI-A2N — R12 Production Cutover Validation

Status:

```text
R12-A COMPLETE
R12-B COMPLETE / VALIDATED
R0-R12 COMPLETE / VALIDATED
Native HUD = production default
Legacy assets retained
R13 NOT STARTED
```

Date: **2026-09-01**
Branch: `main`

> **Path relocation note:** On 2026-09-01 the retained deprecated Legacy assets
> were moved to `/Game/SlayTheSpireDemo/UI/Out/Legacy/`. Historical paths below
> describe the validated repository state at that time.

## Preconditions and cleanup

R11 was formally `COMPLETE / VALIDATED` before R12 began. The temporary R11 PIE
command source and its sole test-module `UnrealEd` dependency had already been
removed in commit `851717e`; the cleanup Editor build passed and the worktree was
clean before the production cutover.

No R11-only harness was carried into the R12-A commit.

## R12-A — isolated production cutover

The unique `ABattleHUDPresenter::WidgetClass` instance in the production map was
changed from Legacy to Native:

```text
Map:
/Game/SlayTheSpireDemo/Maps/L_BattleTest

Before:
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD.WBP_BattleHUD_C

After:
/Game/SlayTheSpireDemo/UI/Widgets/WBP_BattleHUD_Native.WBP_BattleHUD_Native_C
```

The saved-map reopen check confirmed the Native class. The isolated commit changed
only `Content/SlayTheSpireDemo/Maps/L_BattleTest.umap`:

```text
de788c5b68e06827f8fdba3b83858f86a385bdeb
feat(ui-a2n): cut production HUD over to native
```

No Runtime/C++, Gameplay, Presentation, Controller/reducer, Record/Envelope,
Legacy asset, Native implementation, production Widget asset, cleanup or UI-A3
change was included in R12-A.

## R12-B — automated gates

All formal cutover acceptance evidence was executed from the R12-A production
configuration. Passing Gates were not repeated unless the temporary manual PIE
harness cleanup specifically invalidated Build/Shipping evidence.

```text
SlayTheSpireDemoEditor Win64 Development: PASS

WBP_BattleHUD_Native:    compile PASS / save PASS / reopen PASS
WBP_BattleCard_Native:   compile PASS / save PASS / reopen PASS
WBP_BattleStatus_Native: compile PASS / save PASS / reopen PASS
Blueprint status after reopen: BS_UP_TO_DATE for all three
Blueprint/Designer errors: 0
Legacy assets dirtied: 0

SlayTheSpireDemo.Phase6UIA2D5:
exactly 6/6 Success, 0 failed, 0 notRun

Formal Phase6R current-head workflow:
Phase5 13/13
Phase6A 23/23
Phase6B 12/12
Phase6C 5/5
Phase6UIA2A 8/8
Phase6UIA2B 8/8
Phase6UIA2C 8/8
Phase6UIA2D1 3/3
Phase6UIA2D2 4/4
Phase6UIA2D3 4/4
Phase6UIA2D4 6/6
Phase6UIA2D5 6/6
Aggregate: exactly 100/100 Success, 0 failed, 0 notRun

Clean-worktree SlayTheSpireDemo Win64 Shipping: PASS
SlayTheSpireDemoTests / Phase6ATest forbidden artifact hits: 0
Runtime testing/harness hits: 0
```

The formal Phase6R result came from twelve freshly executed prefix reports in one
R12-B workflow; it was not assembled from historical results. The initial and
post-harness-cleanup Shipping checks both used clean detached worktrees.

## R12-B — manual PIE gates

The user completed the complete manual Gate set on **2026-09-01** in the formal
production map:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest
WidgetClass = WBP_BattleHUD_Native_C
```

Confirmed PASS:

```text
Scenario A — real Strike / Energy / Enemy Damage / card-zone ordering
Scenario B — real Uppercut Status identity and 2 -> 1 -> 0 lifecycle
Scenario C — full EndTurn discard / enemy Damage / draw / shuffle macro
Scenario D — real Victory and Defeat terminal ordering and labels
Scenario E — authoritative ResolutionFault / PresentationUnavailable separation
active Skip — immediate visual cleanup and FinalSnapshot catch-up
active Cancel — exact timeout Cancel with later Record continuation
stale callback — Token A no-op while Token B remained active
Input Unlock — normal Finish, Skip catch-up, Cancel catch-up and EndTurn macro
```

The temporal harness logs additionally proved:

```text
[R12 PIE][TestSkip] PASS
[R12 PIE][TestCancelStale] Stale Token A was a no-op; Token B remains active.
[R12 PIE][TestCancelStale] PASS
```

The user confirmed the required visual observations: no `A -> B -> A` flashback,
duplicate Hand/Status/Damage, abandoned transient return, terminal rollback,
premature unlock or permanent Input Lock.

## Temporary harness cleanup

The R12 manual Gate temporarily used the Editor-test-only source
`Phase6UIA2NR12PIECommands.cpp` to expose genuine producer/timeout paths. It did not
change Runtime APIs or production semantics and was never committed. After manual
acceptance it was deleted, then the affected Editor build and clean-worktree
Shipping exclusion Gate were rerun and passed:

```text
Final cleanup verification HEAD: 2fc9f7703bb8bb45e2f75b8f740e646137af0d57
Editor Build: PASS
Shipping Build: PASS
Temporary harness source files: 0
Forbidden Shipping artifact hits: 0
Runtime testing hits: 0
```

## Final state

R12 has no remaining blocker. Native HUD is the production default; Legacy
HUD/Card/Status assets remain retained as the fallback inventory. No Legacy cleanup,
asset deletion, R13 stabilization work, R14 cleanup or UI-A3 work was started.
