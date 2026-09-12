# G8-E Execution — Integration Validation / Cleanup

Date: 2026-09-12

Status: **COMPLETE / VALIDATED**

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

G8-E preserved the validated G8-D external contract:

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

Completed on main. UE5.8 Development Editor build **PASS** reported on 2026-09-12.

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

Final behavior:

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

Final current-main static inspection confirms no `CompatibilityDebt` production symbol remains in:

```text
BattlePresentationController.h
BattlePresentationController.cpp
BattlePresentationControllerG8C.cpp
```

Historical G8-C design/evidence documentation intentionally retains compatibility-debt terminology because it describes the earlier staging contract; G8-E does not rewrite historical evidence.

## E Automation validation

The five required G8-E integration Automation reports were run after the E2 build. The exported `index.json` reports all showed `failed: 0`, with no test-level errors.

Validated suites:

```text
SlayTheSpireDemo.SelectionPresentation.G8D       PASS / 0 failed
SlayTheSpireDemo.SelectionPresentation.G8B       PASS / 0 failed
SlayTheSpireDemo.Phase6UIA2N.FastInput           PASS / 0 failed
SlayTheSpireDemo.SelectionPresentation.G8A       PASS / 0 failed
SlayTheSpireDemo.CardSelection.Unified           PASS / 0 failed
```

Local exported evidence paths:

```text
Saved/AutomationReports/G8E/SlayTheSpireDemo_SelectionPresentation_G8D/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_SelectionPresentation_G8B/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_Phase6UIA2N_FastInput/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_SelectionPresentation_G8A/index.json
Saved/AutomationReports/G8E/SlayTheSpireDemo_CardSelection_Unified/index.json
```

HTML template matches for words such as `Error` / `Failed` are not failures; the authoritative exported report field is the per-suite `index.json` summary.

## E manual PIE acceptance

Manual PIE was user-accepted on 2026-09-12 after validating the G8 boundary and clarifying the remaining Blocking card-tail behavior.

The accepted G8-D/G8-E contract is:

```text
DamageNumber alive
+ authoritative chronology caught up
→ input may become available
→ pure cosmetic tail is not skippable presentation delay
→ DamageNumber does not own formal HP/Block or readiness
```

Observed Hand hover remaining paused while a real `CardPlayed` / `CardZoneChanged` card Presentation is still active is **not** a G8 failure. The Native Hand interaction path intentionally suppresses fan-hover updates during tracked/native Blocking Presentation. Full card-tail overlap and buffered next-card input remain a G9 non-goal.

The user explicitly accepted G8-E after this scope check. No new Gameplay/reducer divergence, ghost DamageNumber, rollback, or permanent input lock was reported during the final acceptance pass.

## E acceptance

Final acceptance:

```text
E1/E2 structural cleanup builds cleanly                         PASS
focused + affected Automation are green                       PASS
manual integration / PIE accepted by user                     PASS
no compatibility-debt runtime state/helper remains            PASS
G8-D NonBlocking DamageNumber contract retained               PASS
G9 non-goals remain untouched                                 PASS
```

Therefore:

```text
G8-E — COMPLETE / VALIDATED
```

Proceed to **G8-F — Evidence / Seal**. G8-F records final evidence and freezes the G8 implementation; it introduces no new Presentation behavior.
