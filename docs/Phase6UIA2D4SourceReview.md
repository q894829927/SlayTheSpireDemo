# Phase 6UI-A2D4 Static Source Review

Date: **2026-08-22**

Status: **IMPLEMENTED / STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

## Review scope

Reviewed A2D-4 additions:

```text
typed Victory / Defeat payload
 typed ResolutionFault payload
legacy root fault-field removal
Victory / Defeat producer identity freezing
ResolutionFault typed producer
terminal envelope shape validation
terminal WorkingPresentationSnapshot reducer
pre-Blueprint terminal preflight
terminal visible playback participation
callback / false fallback / timeout completion
skip / stale-token behavior
A2A generic playback probe migration
focused Automation and Phase6R count update
```

A2D-1 through A2D-3 remain the validated baseline and were not redesigned.

## 1. Type ownership

`FPresentationRecord` now has one terminal payload truth:

```text
Terminal
ResolutionFault
```

The old root fields:

```text
FaultReason
FaultExecutedActionCount
FaultLastActionName
```

were removed from runtime source. Repository search found no remaining runtime/test references to those legacy root field names; only design/history documentation still mentions them as removed fields.

## 2. Producer review

### Victory / Defeat

`ABattleManager::CheckBattleResult()` resolves terminal participant identities through `TryResolveCombatantPresentationId()`.

Required producer invariants:

```text
winner non-empty
defeated non-empty
winner != defeated
Victory: Player wins / Enemy defeated
Defeat: Enemy wins / Player defeated
```

If Gameplay terminal state is already committed but identity freezing fails, the unpublished Presentation batch is invalidated without rolling Gameplay back.

The implementation originally produced a large formatting-only diff in `BattleManager.cpp`; this was corrected by commit `7807889f`, leaving only the intended terminal producer changes relative to the pre-A2D4 source shape.

### ResolutionFault

Framework fault remains authoritative in `BattleActionQueue` / `HandleActionQueueResolutionFaulted`. Presentation freezes:

```text
Reason
ExecutedActionCount >= 0
LastActionName
```

into `FResolutionFaultPresentationPayload`.

## 3. Recorder boundary

The existing Recorder terminal invariant remains unchanged and compatible with A2D-4:

```text
terminal Record is final
only one terminal Record
append after terminal invalidates unpublished batch
```

A2D-4 does not make `PresentationUnavailable` a terminal Gameplay fault.

## 4. Controller envelope preflight

`ValidateTerminalEnvelopeShape()` is applied before an Envelope begins incremental playback.

It rejects:

```text
multiple terminal Records
terminal Record not last
terminal Record with non-terminal FinalSnapshot
terminal FinalSnapshot with no terminal Record
terminal Record type inconsistent with FinalSnapshot state/outcome
```

This prevents the Controller from playing a prefix of history whose terminal contract is already known to be corrupt.

## 5. Terminal reducer review

`ApplyTerminalRecord()` runs against a temporary WorkingSnapshot during preflight and against the real WorkingSnapshot only after playback completion.

Victory and Defeat require the defeated participant to already be dead in WorkingSnapshot. Therefore terminal playback cannot hide a missing lethal Damage history entry.

The reducer changes only:

```text
BattleState
Outcome
bCanEndTurn
```

It does not synthesize HP, Block, Status, Energy or card-zone state.

ResolutionFault does not require a dead participant and validates typed diagnostics plus terminal FinalSnapshot state/outcome.

## 6. Playback timing review

For terminal Records:

```text
preflight copy
→ visible playback
→ callback / false fallback / timeout
→ real WorkingSnapshot terminal commit
→ ViewModel.ApplyPresentationSnapshot
→ Terminal interaction state
→ exact FinalSnapshot reconciliation
```

The real WorkingSnapshot is not advanced during terminal animation.

`SkipPresentation()` remains a direct catch-up operation and may enter terminal state immediately from FinalSnapshot. Existing PlaybackToken generation checks make callbacks from the pre-skip generation stale.

## 7. Existing A2A compatibility

Pre-A2D4 A2A tests used `ResolutionFault` as a generic visible-record probe. With formal terminal semantics that would create invalid synthetic history.

Those generic probes were migrated to no-state-change `BlockChanged` records. Real framework fault tests remain real `ResolutionFault` scenarios and now inspect the typed payload where relevant.

No A2A top-level test count changed.

## 8. Focused A2D-4 coverage

Six top-level tests are defined:

```text
Producer.VictoryPayload
Producer.DefeatPayload
Producer.ResolutionFaultPayload
Playback.TerminalCompletionTiming
Playback.TerminalTimeout
Safety.PreflightFallbackSkip
```

The source coverage includes:

```text
typed producer identity/diagnostics
Damage → Victory ordering
non-terminal display while Victory playback is active
terminal commit after callback
invalid terminal rejected before Blueprint
native false fallback
terminal timeout
skip to FinalSnapshot
stale callback after skip ignored
Presentation recovery does not fault Gameplay
```

## 9. CI contract

Dedicated A2D-4 gate:

```text
SlayTheSpireDemo.Phase6UIA2D4
ExpectedCount = 6
```

Updated Phase6R total:

```text
94
```

The Shipping exclusion job remains chained after regression success.

## 10. Static review result

No high-confidence C++/UHT structural blocker was found by source inspection.

The UE5.8 `FAutomationTestBase` API used by the new tests includes `TestTrue`, `TestFalse`, `TestEqual` and templated `TestNotNull`, so the helper/test assertion style is compatible with UE5.8 Automation APIs.

Static review is not a substitute for the authoritative engine build/run. Current status remains **UE5.8 VALIDATION PENDING**.

Required next gates:

```text
UE5.8 Editor build
A2D4 6/6
A2D1 3/3
A2D2 4/4
A2D3 4/4
Phase6R 94/94
Shipping exclusion PASS
```
