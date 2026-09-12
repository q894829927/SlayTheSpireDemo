# G8-D Execution — True NonBlocking DamageNumber

Date: 2026-09-12

Status: **COMPLETE / VALIDATED ON MAIN**

G8-D removes the G8-C compatibility-wait behavior from production readiness while preserving the detached Damage transaction, exact Presentation session fencing, Blocking fallback, runtime-disable fallback, and one-way cosmetic ownership.

## Production contract

After an eligible detached Damage commit:

```text
formal Damage reducer commit
→ publish HP / Block
→ activate DamageNumber best-effort cosmetic
→ advance chronological Record / Envelope
→ when the exact interaction surface is otherwise caught up, restore input immediately
```

There is no longer a Damage compatibility wait between chronological completion and input readiness.

The DamageNumber lifetime is cosmetic only:

```text
DamageNumber alive
X→ HasSkippablePresentationDelay
X→ FastInput catch-up credential
X→ ViewModel readiness barrier
X→ Controller Record completion
```

A pure DamageNumber tail therefore cannot cause `SkipPresentation()` through FastInput.

## Preserved contracts

- Detached prepare decline still falls back to the existing Blocking Damage path before formal commit.
- After formal commit, stale/failed cosmetic activation never replays the reducer and never falls back to Blocking Damage.
- Runtime detached-feature disable keeps the current PresentationSessionToken when authority/binding is unchanged, cancels current-session detached cosmetics, and sends future Damage through the old Blocking path.
- Ordinary Skip may still cancel current-session detached cosmetics, but a pure cosmetic tail is not itself a reason to enter Skip.
- HUD/Controller/battle/authority replacement still invalidates the old Presentation session and retires old-session cosmetics.
- CardPlayed/CardZoneChanged/card-tail Presentation remains Blocking. Buffered card input and detached card presentation remain G9 non-goals for G8.

## G8-C debt migration

G8-D production behavior has already removed compatibility debt from readiness/FastInput semantics:

```text
CompatibilityDebtSeconds is never accrued
CompatibilityDebtTimer is never started
HasCompatibilityDebt() is false
HasSkippablePresentationDelay() ignores debt/cosmetic lifetime
```

The Controller debt helper call sites were intentionally retained as temporary migration shims during D validation so the semantic change and the structural cleanup were not mixed in one checkpoint. G8-E owns physical cleanup of that obsolete surface before final integration seal.

## Focused Automation — PASS

Suite:

```text
SlayTheSpireDemo.SelectionPresentation.G8D
```

Validated cases:

```text
DetachedDamage.NonBlockingReadiness
DetachedDamage.PrepareDeclineFallsBack
RuntimeDisable.KeepsSession
Cosmetic.WidgetReplacement
```

Result reported on the validated main line:

```text
4/4 PASS
0 failed
```

The old G8-C debt-specific Automation files were retired because their required contract (`Damage → compatibility debt`) is intentionally removed by G8-D. G8-C validation remains historical evidence rather than a current production expectation.

## Affected regression validation — PASS

The following affected suites were rerun after G8-D and reported passing:

```text
SlayTheSpireDemo.SelectionPresentation.G8B      9/9 PASS
SlayTheSpireDemo.Phase6UIA2N.FastInput          2/2 PASS
SlayTheSpireDemo.SelectionPresentation.G8A      4/4 PASS
SlayTheSpireDemo.CardSelection.Unified         14/14 PASS
```

## Validated G8-D checkpoint

The accepted G8-D checkpoint is therefore:

```text
Damage formal reducer remains authoritative
DamageNumber is detached cosmetic only
pure DamageNumber tail does not create FastInput Skip eligibility
readiness no longer waits for legacy Damage duration
prepare-decline still falls back to Blocking
runtime disable preserves SessionToken and restores Blocking fallback
Widget replacement retires old-session cosmetic without re-locking ready input
no G9 card-presentation overlap behavior is introduced
```

G8-D is now handed off to **G8-E — Integration Validation / Cleanup**. G8-E may remove the temporary compatibility-debt migration surface, but must preserve the validated G8-D external behavior above.
