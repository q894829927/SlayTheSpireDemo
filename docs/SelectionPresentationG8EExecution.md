# G8-E Execution — Integration Validation / Cleanup

Date: 2026-09-12

Status: **IN PROGRESS — BUILD + AUTOMATION PASS / MANUAL PIE PENDING**

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

Implemented on main. UE5.8 Development Editor build **PASS** reported on 2026-09-12.

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

## E Automation validation

The five required G8-E integration Automation reports were run after the E2 build. The exported `index.json` reports all show `failed: 0`, with no test-level errors.

Validated suites:

```text
SlayTheSpireDemo.SelectionPresentation.G8D       PASS / 0 failed
SlayTheSpireDemo.SelectionPresentation.G8B       PASS / 0 failed
SlayTheSpireDemo.Phase6UIA2N.FastInput           PASS / 0 failed
SlayTheSpireDemo.SelectionPresentation.G8A       PASS / 0 failed
SlayTheSpireDemo.CardSelection.Unified           PASS / 0 failed
```

HTML template matches for words such as `Error` / `Failed` are not failures; the authoritative exported report field is the per-suite `index.json` summary.

## E manual PIE integration matrix

Automation is green. Manual PIE validation is now the only remaining G8-E acceptance gate.

Run the following against current main:

```text
[ ] single Damage: HP/Block updates once; DamageNumber is cosmetic and does not hold input
[ ] fully-blocked Damage: Block reduction is correct; no HP rollback; DamageNumber/cues remain sane
[ ] consecutive / multi-hit Damage: each formal hit commits exactly once; multiple cosmetics may coexist; no stuck input
[ ] Damage → OtherBlocking → Damage: only the Blocking presentation holds chronology/input
[ ] rapid legal next-card input while old DamageNumber is alive: no FastInput Skip caused by cosmetic tail
[ ] player Hit / enemy Hit / enemy Attack cues remain best-effort visual-only and do not own readiness
[ ] ReadyToConfirm confirm/cancel and legal card switch still obey exact selection boundary
[ ] Target choose/cancel and legal card switch still obey exact target boundary
[ ] TargetChoice EndTurn rejection remains correct
[ ] Draw → PendingSelection boundaries remain ordered and usable
[ ] ordinary reconcile preserves backlog continuity and current PresentationSessionToken
[ ] HUD replacement cleans old detached cosmetics, mints replacement session, and does not leave input locked
[ ] Controller replacement / battle replacement invalidate old session ownership
[ ] runtime detached-feature disable keeps current SessionToken, clears current cosmetics, and routes later Damage through Blocking fallback
[ ] DirectBaseline / PresentationUnavailable / terminal transitions have no ghost cosmetic or stuck input
[ ] HUD deactivation/destruction / viewport-DPI changes leave no ghost DamageNumber
[ ] no duplicate formal Damage, no HP/Block rollback, no stale callback mutation, no permanent input lock
```

The key G8-D/G8-E acceptance observation remains:

```text
DamageNumber alive
+ authoritative chronology caught up
→ input is available
→ pure cosmetic tail is not skippable presentation delay
→ playing the next legal card does not cancel/skip the old DamageNumber merely to accept input
```

CardPlayed/CardZoneChanged animation overlap with buffered next-card intent remains explicitly out of scope and belongs to G9.

## E acceptance

G8-E can become `COMPLETE / VALIDATED` only when:

```text
E1/E2 structural cleanup builds cleanly                         PASS
focused + affected Automation are green                       PASS
manual integration matrix has no new divergence               PENDING
no compatibility-debt runtime state/helper remains            PASS
G8-D NonBlocking behavior is unchanged                        PENDING PIE CONFIRMATION
G9 non-goals remain untouched                                 PASS
```

After G8-E completes, proceed to **G8-F — Evidence / Seal**. G8-F records the final build, Automation and PIE evidence and freezes the G8 implementation; it does not introduce new presentation behavior.
