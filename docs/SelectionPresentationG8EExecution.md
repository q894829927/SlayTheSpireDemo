# G8-E Execution — Integration Validation / Cleanup

Date: 2026-09-12

Status: **IN PROGRESS — E2 CLEANUP IMPLEMENTED / VALIDATION PENDING**

## Entry condition

G8-D was accepted as the validated behavioral baseline before entering G8-E.

Recorded validated suites from the G8-D checkpoint:

```text
SlayTheSpireDemo.SelectionPresentation.G8D       4/4 PASS
SlayTheSpireDemo.SelectionPresentation.G8B       9/9 PASS
SlayTheSpireDemo.Phase6UIA2N.FastInput           2/2 PASS
SlayTheSpireDemo.SelectionPresentation.G8A       4/4 PASS
SlayTheSpireDemo.CardSelection.Unified          14/14 PASS
```

G8-E must not change the validated G8-D external contract:

```text
formal Damage reducer remains authoritative
DamageNumber is detached cosmetic only
pure DamageNumber tail never grants FastInput Skip eligibility
input readiness never waits for DamageNumber lifetime
prepare-decline still falls back to legacy Blocking Damage
runtime feature disable keeps SessionToken when authority/binding is unchanged
HUD/battle/Controller/authority replacement invalidates the old session
CardPlayed/CardZoneChanged remain Blocking; buffered card input is G9
```

## E1 — Remove physical compatibility-debt state

Completed on main and UE5.8 Development Editor build **PASS** was reported before E2 began.

Completed cleanup:

- removed `CompatibilityDebtSeconds` from `UBattlePresentationController`
- removed `CompatibilityDebtTimerHandle`
- removed Controller-header dependency on `TimerManager.h`
- removed debt testing accessors from the public test surface
- removed TimerManager/world debt servicing code from the detached-Damage implementation file
- updated G8-D tests to validate only external NonBlocking behavior instead of obsolete debt internals

E1 intentionally retained stateless debt-named call-site migration wrappers so physical state removal could be compiled independently before chronology call sites were cleaned.

## E2 — Remove debt-named call-site migration wrappers

Implemented on main; compile/regression validation is pending.

Removed production helper surface:

```text
AddCompatibilityDebtForCommittedDamage
PauseCompatibilityDebtService
TryServiceCompatibilityDebtOrRefreshInput
HandleCompatibilityDebtElapsed
ClearCompatibilityDebt
HasCompatibilityDebt
IsExactReadSurfaceCaughtUpForDebtService
```

Replacement production helper:

```text
RefreshInputIfPresentationCaughtUp
```

Current behavior:

```text
Damage formal commit
→ no timing bookkeeping

accepted/new chronology
→ no debt pause operation

chronology/read-ready completion
→ RefreshInputIfPresentationCaughtUp
→ ViewModel RefreshLiveInputBindingsIfCaughtUp remains the exact read/revision guard

Skip/session invalidation/replacement
→ cosmetic cleanup only; no debt cleanup state exists

SetWidget in-flight detection
→ active envelope / playback queue only
```

Static current-main inspection confirms no `CompatibilityDebt` symbol remains in:

```text
BattlePresentationController.h
BattlePresentationController.cpp
BattlePresentationControllerG8C.cpp
```

Historical G8-C design/evidence documentation intentionally retains compatibility-debt terminology because it describes the earlier staging contract; G8-E does not rewrite historical evidence.

E2 is structural cleanup only. It does not authorize changes to SessionToken rules, reducer order, Blocking fallback, FastInput fencing, card Presentation overlap, or ViewModel readiness guards.

## E integration matrix

After E2 build passes, rerun at minimum:

```text
SlayTheSpireDemo.SelectionPresentation.G8D
SlayTheSpireDemo.SelectionPresentation.G8B
SlayTheSpireDemo.Phase6UIA2N.FastInput
SlayTheSpireDemo.SelectionPresentation.G8A
SlayTheSpireDemo.CardSelection.Unified
```

Additional G8-E manual integration coverage follows the frozen design:

```text
single Damage / fully-blocked Damage
multi-hit / consecutive Damage
Damage → OtherBlocking → Damage
player Hit / enemy Hit / enemy Attack cues
rapid legal card input
ReadyToConfirm confirm/cancel + legal card switch
Target choose/cancel + legal card switch
TargetChoice EndTurn rejection
Draw → PendingSelection boundaries
FastInput exact revision/session fencing
ordinary reconcile backlog continuity
HUD replacement / Controller replacement / battle replacement
DirectBaseline / unavailable transition / terminal
runtime detached-feature disable → Blocking fallback
viewport/DPI / HUD deactivation/destruction cleanup
no ghost DamageNumber / no rollback / no stuck input
```

## E acceptance

G8-E can become `COMPLETE / VALIDATED` only when:

```text
E1/E2 structural cleanup builds cleanly
focused + affected Automation are green
manual integration matrix has no new divergence
no compatibility-debt runtime state or debt-named production helper remains
G8-D NonBlocking behavior is unchanged
G9 non-goals remain untouched
```

After G8-E completes, proceed to **G8-F — Evidence / Seal**. G8-F records the final build, Automation and PIE evidence and freezes the G8 implementation; it does not introduce new presentation behavior.
