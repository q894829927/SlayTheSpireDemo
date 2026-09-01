# Phase 6UI-A2N — R10 Native Terminal / PresentationUnavailable

Status:

```text
R0-R9 COMPLETE / VALIDATED
R10 SOURCE IMPLEMENTED / FOCUSED AUTOMATION 5/5 PASS / MANUAL PIE PENDING
R11 NOT STARTED
```

Implementation branch: `main` (continuing the explicitly authorized direct-main A2N workflow)
Starting main HEAD: `bab10acdfd9939e7122506391e73488584e7bb71`
Implementation date: **2026-09-01**

R10 migrates only the formal Native HUD rendering of committed terminal Records:

```text
Victory
Defeat
ResolutionFault
```

and makes the already-existing `PresentationUnavailable` ViewModel state an explicit
pure Native rendering boundary. R10 does not change Gameplay, Controller, reducer,
Presentation Record/Envelope schema, production WidgetClass, Legacy WBP assets,
UI-A3, or R11+ behavior.

## Terminal ordering and authority

Terminal Records reuse the sealed R5 exact-token playback kernel. The Native HUD
accepts a terminal Record only when the historical ViewModel still has
`Outcome == None`; it never reads a future Gameplay Outcome to reveal the terminal
surface early.

The visible ordering remains:

```text
preceding committed visual Records complete
-> Controller reduces those Records into the historical ViewModel
-> Terminal Record reaches visual head
-> Native terminal surface appears
-> exact terminal Finish / Notify
-> Controller reduces the Terminal Record
-> FinalSnapshot reconciliation
```

Victory additionally requires the historical Enemy to already be dead and exact
terminal identities `Player -> Enemy`. Defeat requires the historical Player to
already be dead and exact identities `Enemy -> Player`. This keeps lethal Damage
visible/reduced before the terminal surface is accepted.

ResolutionFault requires a non-empty frozen `Reason` and
`ExecutedActionCount >= 0`. It does not derive a reason and does not manufacture a
Gameplay fault.

## Formal visible surfaces

The Native terminal helper renders only the three sealed outcomes:

```text
Victory            -> 胜利
Defeat             -> 战斗失败
ResolutionFaulted  -> 战斗结算异常
None               -> terminal overlay collapsed / outcome text empty
```

Normal terminal Finish retains the committed terminal visual until Controller
reduction and ViewModel refresh formally own the same state.

Exact Cancel does not infer an inverse terminal transition. It redraws the terminal
surface from the current historical ViewModel; because the active Terminal Record
has not yet been reduced, this restores the historical pre-terminal surface and
never sends a normal completion Notify.

Wrong/stale Token callbacks remain no-ops through the R5 kernel. Native destruction
only clears local timer/ownership through the existing teardown path and does not
perform historical restore or normal Notify.

## PresentationUnavailable separation

`PresentationUnavailable` remains a ViewModel-driven state, not a Record:

```text
Controller / Presenter
-> ViewModel.EnterPresentationUnavailable(...)
-> ViewModel Changed
-> Native HUD pure rendering
```

The Native HUD does not call `EnterPresentationUnavailable`, derive its reason,
generate `ResolutionFault`, or route it through terminal Record playback.

`RefreshPresentationAvailabilityFromViewModel()` consumes only the ViewModel state.
When the interaction state is `PresentationUnavailable`, it:

```text
keeps the formal feedback reason visible
collapses the terminal overlay
clears terminal outcome text
relies on the existing ViewModel input-lock state / RefreshInputState
```

This explicitly prevents a Presentation failure from being rendered as the Gameplay
`战斗结算异常` terminal outcome.

## Focused Automation — PASS

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R10
```

Five Editor-only tests:

```text
Terminal.VictoryOrderingAndFinish
Terminal.DefeatCancelHistoricalRestore
Terminal.ResolutionFaultValidation
PresentationUnavailable.SeparateAndLocked
Terminal.InvalidIdentityAndDestruct
```

Deterministic coverage includes:

- Victory rejection before historical lethal Enemy state and acceptance after it;
- Defeat historical lethal Player requirement;
- exact terminal identity checks;
- terminal Begin never mutates ViewModel Outcome;
- stale Finish no-op;
- wrong-token Cancel no-op;
- exact Cancel historical terminal restore;
- ResolutionFault reason/count validation;
- PresentationUnavailable is not ResolutionFault and keeps input disabled;
- PresentationUnavailable hides any stale terminal overlay and renders ViewModel feedback;
- terminal Record playback is rejected while PresentationUnavailable is active;
- NativeDestruct clears local terminal ownership/timer without historical restore.

The first focused run after the R10 source implementation found one fixture/precondition
error in `PresentationUnavailable.SeparateAndLocked`: the test manufactured a stale
`ResolutionFaulted` terminal visual and then incorrectly expected
`EnterPresentationUnavailable()` itself to clear the pre-existing ViewModel Outcome.
The sealed ViewModel contract does not mutate Outcome in that method. The test was
corrected to restore `Outcome=None` before entering PresentationUnavailable while
intentionally leaving the stale visual on screen, so the test now proves both that
PresentationUnavailable does not manufacture a Gameplay fault and that the Native HUD
hides the stale terminal surface.

The corrected focused suite was then rerun locally on UE5.8 and the user confirmed:

```text
SlayTheSpireDemo.Phase6UIA2N.R10: 5/5 PASS
```

The uploaded failed-run log is retained only as correction history; it showed the
other four tests already passing and the single fixture assertion failure that led to
the correction. It is not the final focused Gate.

## AUTOMATED GATES

Required closed-scope gates are:

```text
1. SlayTheSpireDemoEditor Win64 Development build
2. SlayTheSpireDemo.Phase6UIA2N.R10 focused Automation
```

Focused Automation: **5/5 PASS** (user confirmed 2026-09-01 after the corrected test
fixture).

Final Build evidence after the last test-source correction is not newly claimed by
this document until explicitly confirmed. Do not infer an unreported Build result
from the Automation result.

Targeted `WBP_BattleHUD_Native` compile is **NOT REQUIRED** for this R10 source head
because R10 adds no new `UPROPERTY`, `UFUNCTION`, `BindWidget`, class selector, or
other reflected Designer contract. The existing terminal/feedback bindings were
already validated in earlier Native phases.

Do not automatically run R3-R9, A2D5, Phase6R, Shipping, Scenario A-E, all-Blueprint
compilation, or an architecture reviewer.

## MANUAL PIE GATE — PENDING

Use:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

R10 explicitly requires one focused visual check per genuinely distinct terminal
surface. Use the existing real producers/harnesses and verify only:

```text
Victory:
  lethal Enemy Damage reaches visible 0 HP first
  then formal overlay shows 胜利

Defeat:
  lethal Player Damage reaches visible 0 HP first
  then formal overlay shows 战斗失败

ResolutionFault:
  isolated real resolution fault shows 战斗结算异常

PresentationUnavailable:
  presentation failure shows its ViewModel feedback/error state
  does NOT show 战斗结算异常 terminal overlay
  input remains locked
```

Also confirm no terminal flashback/duplicate and no early input unlock. Do not replay
the broad Scenario A-E parity matrix; that belongs to R11.

## Current acceptance state

```text
R10 SOURCE IMPLEMENTED
FOCUSED AUTOMATION 5/5 PASS
FINAL BUILD CONFIRMATION NOT YET RECORDED AFTER LAST TEST-SOURCE CORRECTION
MANUAL PIE PENDING
R11 NOT STARTED
```

Do not mark R10 `COMPLETE / VALIDATED` or start R11 until the remaining required
Build evidence and manual Gate are actually confirmed.
