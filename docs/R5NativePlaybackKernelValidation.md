# Phase 6UI-A2N — R5 Native Playback Kernel

Status: **SOURCE IMPLEMENTED / UE VALIDATION PENDING**

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

## Focused Editor-only Automation added

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

## AUTOMATED GATES — USER ACTION REQUIRED

These have **not** been run by this GitHub-only implementation session. Do not mark them PASS until executed locally in UE5.8.

### Gate A — Editor Build

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Build\BatchFiles\Build.bat" `
  SlayTheSpireDemoEditor Win64 Development `
  -Project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -WaitMutex -NoHotReload
```

Expected: `Result: Succeeded`.

### Gate B — targeted Native Blueprint compile

Because R5 changes the inherited Native HUD C++ contract, compile only:

```text
WBP_BattleHUD_Native
```

Expected:

```text
no inherited member collision
no BindWidget error
no Blueprint compile error
```

Do not run `CompileAllBlueprints` for this ordinary R5 gate.

### Gate C — focused R5 Automation

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  "E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -ExecCmds="Automation RunTests SlayTheSpireDemo.Phase6UIA2N.R5; Quit" `
  -unattended -nopause `
  -testexit="Automation Test Queue Empty" `
  -log
```

Expected:

```text
4 tests discovered
4 PASS
0 failed
0 notRun
```

Passing Gates are sticky. If one Gate fails, fix and rerun only the Gate(s) invalidated by that fix according to `docs/ValidationExecutionPolicy.md`.

## MANUAL PIE GATE — PENDING

After A/B/C pass, run one minimal smoke only:

```text
Map: /Game/SlayTheSpireDemo/Maps/L_BattleTest_Native

1. Start PIE.
2. Confirm the battle HUD and Hand appear normally.
3. Select one card, then Cancel.
4. If convenient, perform one normal EndTurn.
5. Confirm:
   - no crash;
   - no permanent input lock;
   - no duplicate Hand;
   - no blank/abandoned HUD;
   - immediate fallback still permits continued interaction.
```

Do not test R6+ visuals as R5 acceptance. Missing Damage/CardPlayed/Energy/Block/Status/Terminal animations are expected at this stage.

## Acceptance state

Current:

```text
R5 SOURCE IMPLEMENTED
AUTOMATED VALIDATION PENDING
MANUAL PIE PENDING
R6 NOT STARTED
```

Only after the required local gates actually pass may the repository checkpoint and trusted validation evidence be advanced to `R5 COMPLETE / VALIDATED`.
