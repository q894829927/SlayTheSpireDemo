# Phase 6UI-A2N — R7 Native Damage

Status:

```text
R7 SOURCE IMPLEMENTED
AUTOMATED VALIDATION PASS
MANUAL PIE PENDING
R8 NOT STARTED
```

Branch: `a2n/r7-native-damage`
Starting main HEAD: `2264b9e5ba8b6505fffcef5abed21d2d6bdc7611`
Source implementation commit: `c3a345413a87197de8328eb94e6b849d365f5442`
Validation date: **2026-08-31**

R7 migrates only the committed `Damage` presentation Record into the Native HUD.
It reuses the sealed R5 playback kernel and does not modify Controller, Reducer,
Record, Envelope, Gameplay, Legacy WBP assets, the production WidgetClass or any R8+
behavior.

## Implemented Damage boundary

The Native handler follows this order:

```text
validate Record / exact Token metadata
-> validate SourcePresentationId and DamageKind
-> resolve exact TargetPresentationId to Player or Enemy
-> validate frozen HP/Block Before against the historical ViewModel
-> validate the sealed Damage payload invariants
-> prepare exact target/text/progress surfaces
-> CommitNativePresentationOwnership
-> copy frozen Before / After and weak target surfaces locally
-> apply frozen HPAfter / BlockAfter
-> display frozen IncomingDamage
-> target RenderOpacity = 0.45
-> start the value-captured exact-token 0.5 s timer
```

The accepted payload invariants are the sealed commit facts:

```text
BlockedDamage = BlockBefore - BlockAfter
HPDamage      = HPBefore - HPAfter
BlockedDamage >= 0
HPDamage >= 0
BlockedDamage + HPDamage <= IncomingDamage
```

The handler does not calculate `HPAfter` or `BlockAfter` from `IncomingDamage`.
Begin, Finish and Cancel render the corresponding frozen Record fields directly.
No mutable Gameplay object is queried.

### Finish

```text
retain frozen HPAfter / BlockAfter
hide Txt_DamagePresentation
restore exact target RenderOpacity = 1.0
clear Damage weak refs and frozen local state
clear timer / local ownership
NotifyPresentationFinished(exact Token) once
```

Stale or duplicate Finish is a no-op and cannot affect a later Record.

### Cancel

```text
restore frozen HPBefore / BlockBefore
hide Txt_DamagePresentation
restore exact target RenderOpacity = 1.0
clear Damage weak refs and frozen local state
clear timer / local ownership
never Notify normal completion
```

Wrong-token Cancel is a no-op.

### Invalid Begin and destruction

Invalid target, payload, Before state or Record/Token metadata returns `false`
before ownership or visible mutation, leaving zero local side effects for Controller
immediate fallback.

`NativeDestruct` clears only the timer, Damage text/opacity feedback, weak target
references and local ownership. It deliberately leaves the currently displayed
frozen After vitals in place, does not historical-restore, and does not Notify.

## Automated Gates — PASS

### Editor Build

```text
SlayTheSpireDemoEditor Win64 Development: PASS
Result: Succeeded
```

No runtime reflected binding/API contract changed. The new reflected probes exist
only in the Editor-only test module, so targeted `WBP_BattleHUD_Native` compile was
not required.

### Focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R7
```

Result:

```text
DestructCleanup:        PASS
EnemyTarget:            PASS
InvalidBegin:           PASS
Lethal:                 PASS
PlayerBlockedAndCancel: PASS

5 succeeded
0 failed
0 notRun
```

Evidence:

```text
Saved/AutomationReports/R7FocusedPhase6UIA2N/index.json
```

Coverage includes Enemy and Player targets, ordinary Damage, full Block absorption,
positive IncomingDamage with unchanged HP, lethal overkill, exact Cancel historical
restore, wrong-token Cancel, stale/duplicate Finish, exact completion cleanup,
next-Record isolation, invalid PresentationId, invalid payload, Before mismatch,
Record/Token mismatch, zero-side-effect false Begin and NativeDestruct cleanup.

No R3-R6, A2D5, Phase6R, Shipping, aggregate regression or architecture reviewer was
run.

## Manual PIE Gate — USER ACTION REQUIRED

Run one minimal PIE pass in:

```text
/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native
```

Required observation:

1. Play `Strike` and select the Enemy.
2. Confirm the Damage number appears exactly once and at the correct target/location.
3. Confirm the Enemy receives one short opacity feedback and returns to normal.
4. Confirm final Enemy HP/Block values are correct.
5. Confirm no flashback, duplicate Damage or permanent Input Lock occurs.

If convenient, also observe one Enemy-to-Player Damage playback with the same target,
cleanup and final-vitals expectations.

This manual Gate is intentionally not replaced with screenshots or additional
Automation.

## Current acceptance state

```text
R0-R6 COMPLETE / VALIDATED
R7 SOURCE IMPLEMENTED
AUTOMATED VALIDATION PASS
MANUAL PIE PENDING
R8 NOT STARTED
```

R7 must not be marked `COMPLETE / VALIDATED` until the user confirms Manual PIE
PASS. Do not start R8 automatically.
