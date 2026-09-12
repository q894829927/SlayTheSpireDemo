# G8-D Execution — True NonBlocking DamageNumber

Date: 2026-09-12

Status: **IMPLEMENTED ON MAIN / VALIDATION PENDING**

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

The existing Controller debt helper call sites are retained temporarily as migration shims so the G8-D patch does not broaden into unrelated Controller refactoring before validation.

In G8-D production behavior:

```text
CompatibilityDebtSeconds is never accrued
CompatibilityDebtTimer is never started
HasCompatibilityDebt() is false
HasSkippablePresentationDelay() ignores debt/cosmetic lifetime
```

The shims synchronously clear any stale staging debt and then use the existing exact ViewModel refresh guard once chronology is otherwise caught up. Physical removal/renaming of the obsolete G8-C helper surface is deferred to G8-E cleanup after D validation.

## Focused Automation

New suite:

```text
SlayTheSpireDemo.SelectionPresentation.G8D
```

Focused cases:

```text
DetachedDamage.NonBlockingReadiness
DetachedDamage.PrepareDeclineFallsBack
RuntimeDisable.KeepsSession
Cosmetic.WidgetReplacement
```

The old G8-C debt-specific Automation files were retired because their required contract (`Damage → compatibility debt`) is intentionally removed by G8-D. G8-C validation remains historical evidence rather than a current production expectation.

## Required validation before G8-D can be marked COMPLETE / VALIDATED

Build:

```text
UE5.8 Development Editor build PASS
```

Focused:

```text
SlayTheSpireDemo.SelectionPresentation.G8D
```

Affected regressions:

```text
SlayTheSpireDemo.SelectionPresentation.G8B
SlayTheSpireDemo.Phase6UIA2N.FastInput
SlayTheSpireDemo.SelectionPresentation.G8A
SlayTheSpireDemo.CardSelection.Unified
```

Manual PIE minimum:

```text
play attack A
→ formal HP/Block updates
→ DamageNumber appears
→ exact normal player surface becomes ready while A DamageNumber is still visible
→ next legal interaction is accepted without Skip caused by A DamageNumber
→ A DamageNumber continues/finishes independently

prepare-decline / runtime-disable fallback
→ old Blocking Damage behavior remains intact

pure cosmetic tail
→ clicking next card does not clear DamageNumber via FastInput Skip

HUD replacement / terminal / destruction
→ old cosmetic cleanup remains exact
→ no ghost DamageNumber
→ no stuck input
```

G8-D must not be marked sealed until these gates pass.
