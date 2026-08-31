# Phase 6UI-A2N — R5 Native Playback Kernel

Status: **COMPLETE / VALIDATED**

Branch: `a2n/r5-native-playback-kernel`
Base: `main@1978e1d3abe831dedef95b8bd431a7717def573b`

R5 establishes only the Native HUD's local committed-presentation playback kernel. It does not migrate any real Record visual; production Native `BeginPresentationRecordPlayback_Implementation` still returns `false` for every Record and therefore preserves the existing Controller immediate-fallback path.

## Implemented kernel

`UBattleHUDWidget` now owns only local visual playback state:

```text
bHasActiveNativePresentation
ActiveNativePresentationType
ActiveNativePresentationToken
NativePresentationFinishTimer
```

No queue, reducer, WorkingSnapshot, Controller generation, Controller timeout authority, Gameplay state or Record/Envelope ownership was copied into the HUD.

Kernel primitives:

```text
CommitNativePresentationOwnership
StartNativePresentationFinishTimer
AbortNativePresentationStart
FinishNativePresentation
ClearNativePresentationFinishTimer
ResetNativePresentationOwnership
CancelPresentationRecordPlayback_Implementation
```

### Begin boundary

R5 itself accepts no real Record type:

```text
BeginPresentationRecordPlayback_Implementation
-> false
-> zero Native local visual side effects
-> existing Controller immediate fallback
```

Later per-Record phases may validate/prepare their resources and use the R5 helpers. If timer preparation fails after ownership was committed, the handler must undo its own visible mutation and call `AbortNativePresentationStart()` before returning `false`.

### Finish boundary

The finish timer captures the exact `FPresentationPlaybackToken` by value. Completion requires:

```text
local active presentation
AND ExpectedToken == ActiveNativePresentationToken
```

Otherwise the callback is a no-op.

Normal kernel completion is:

```text
type-specific Finish hook
-> copy exact Token
-> clear local visual timer
-> clear local active ownership
-> UBattleHUDWidgetBase::NotifyPresentationFinished(exact Token)
```

The Native HUD never calls `UBattlePresentationController::NotifyPresentationFinished` directly.

### Cancel boundary

`CancelPresentationRecordPlayback_Implementation` handles only the exact local active Token:

```text
exact-token check
-> clear visual timer
-> type-specific cancel/restore hook
-> clear local ownership
-> never Notify normal completion
```

Wrong/stale Token cancellation is a no-op.

### Destruction boundary

`NativeDestruct` performs only local teardown before `Super`:

```text
clear Native visual timer
-> cleanup Native presentation-only visuals
-> clear local active ownership
-> existing input/delegate teardown
-> Super::NativeDestruct
```

It does not historical-restore, dispatch the visual Cancel override, or notify normal completion. The base `NotifyWidgetLost` path remains authoritative for catch-up/fail-safe behavior.

## Focused Editor-only Automation

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2N.R5
```

Tests:

```text
UnsupportedAndFailedBegin
ExactCancel
FinishIsolation
DestructCleanup
```

The R5 probe is Editor-only and uses a synthetic accepted `Damage` Record only inside the test subclass. Production `UBattleHUDWidget` does not accept Damage or any other real Record in R5.

Coverage includes:

```text
unsupported production Record returns false
failed synthetic Begin rolls back to zero local state
valid synthetic Begin establishes exact Token + local timer
wrong-token Cancel cannot clear current local owner
public PlayPresentationRecord replacement dispatches exact old-token Cancel
default Cancel never notifies normal completion
duplicate Finish is a no-op
stale Finish cannot clear a newer Token
old deferred completion cannot erase new base/local ownership
exact Finish clears local state before base deferred Notify
NativeDestruct clears local timer/ownership without visual Cancel
```

## Build-failure correction retained as evidence

The first local UE5.8 Editor Build attempt failed at the Native finish-timer delegate binding. `FTimerDelegate::CreateUObject` decayed the bound payload by value while the callback accepted `const FPresentationPlaybackToken&`, so the generated delegate signature could not match the member-function pointer.

The fix remained local to R5:

```text
old:
FTimerDelegate::CreateUObject(this, &UBattleHUDWidget::FinishNativePresentation, ExpectedToken)

new:
FTimerDelegate::CreateWeakLambda(this, [this, ExpectedToken]()
{
    FinishNativePresentation(ExpectedToken);
})
```

`ExpectedToken` remains captured by value, preserving exact-token isolation.

Fix commit:

```text
21e3f7dca0d72c8687465fce10892e205774f893
fix(ui-a2n): bind native finish timer with value-captured token
```

## Final R5 validation evidence — PASS

The user completed the required UE5.8 R5 gates on the corrected branch:

```text
1. SlayTheSpireDemoEditor Win64 Development build: PASS
2. WBP_BattleHUD_Native targeted compile: PASS
3. SlayTheSpireDemo.Phase6UIA2N.R5 focused Automation: PASS
4. L_BattleTest_Native minimal PIE smoke: PASS
```

Focused Automation was run only once after the correction and passed. No R3/R4 or aggregate regression rerun was required.

The manual PIE smoke confirmed:

```text
battle starts normally
Native HUD and Hand are present
card selection -> Cancel returns to an operable state
EndTurn remains usable
no crash
no permanent input lock
no duplicate Hand
no blank/abandoned HUD
immediate fallback remains operable because real Record visuals are still unmigrated
```

This R5 acceptance does not claim Damage, Energy, Block, Shuffle, CardPlayed, CardZoneChanged, StatusChanged or Terminal Record animation parity. Those Record-specific visuals remain later-phase work.

## Explicit R6+ exclusion

R5 does not implement:

```text
EnergyChanged visual
BlockChanged visual
DeckShuffled visual
Damage visual / damage number
CardPlayed visual
CardZoneChanged visual
PlayArea transient cards
StatusChanged lifecycle
Victory / Defeat / ResolutionFault Record visuals
PresentationUnavailable redesign
Controller / Reducer / Record / Envelope changes
Gameplay changes
production cutover
Legacy WBP edits
UI-A3
```

## Acceptance state

```text
R0 COMPLETE / VALIDATED
R1 COMPLETE / VALIDATED
R2 COMPLETE / VALIDATED
R3-A COMPLETE / VALIDATED
R4 COMPLETE / VALIDATED
R5 COMPLETE / VALIDATED
R6 NOT STARTED
```

R5 is closed. The next phase, when explicitly started, is R6 — Energy / Block / Shuffle committed-presentation visuals.