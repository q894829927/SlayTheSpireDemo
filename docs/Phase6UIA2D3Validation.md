# Phase 6UI-A2D3 Validation

Date: **2026-08-22**

Status: **VALIDATED / READY FOR A2D-4**.

Validated A2D-3 source base:

```text
d33d6815d78253a094e8f56448fe32085e191974
docs: record UI-A2D3 static source review
```

At review time `main` is `9efe59f6ef68cbc30d12bae4b403b0ceb39ea946`; the only change after the A2D-3 source-review commit is the Phase 7A workflow registration, so no A2D-3 runtime/test source changed after the reviewed A2D-3 base.

This record captures the user-reported UE5.8 validation result for A2D-3.

## Focused Automation

Prefix:

```text
SlayTheSpireDemo.Phase6UIA2D3
```

Expected and reported result:

```text
Discovered: 4
Succeeded:  4
Failed:     0
NotRun:     0
```

Top-level tests:

```text
SlayTheSpireDemo.Phase6UIA2D3.Snapshot.RuntimeSequenceSorting
SlayTheSpireDemo.Phase6UIA2D3.Playback.StatusLifecycleReducer
SlayTheSpireDemo.Phase6UIA2D3.Safety.StaleRuntimeSequenceCollapses
SlayTheSpireDemo.Phase6UIA2D3.Safety.AmountMismatchCollapses
```

Result: **PASS (4/4)**.

## Affected regression gate

The Phase6R aggregate gate includes:

```text
Phase5          13
Phase6A         23
Phase6B         12
Phase6C          5
Phase6UIA2A      8
Phase6UIA2B      8
Phase6UIA2C      8
Phase6UIA2D1     3
Phase6UIA2D2     4
Phase6UIA2D3     4
------------------
Total           88
```

Reported result: **PASS (88/88)**.

The Phase6R workflow builds `SlayTheSpireDemoEditor Win64 Development` using the configured UE5.8 toolchain before running the regression prefixes. A successful 88/88 execution therefore also satisfies the A2D-3 Editor-build prerequisite.

## Validated A2D-3 contract

The validated source covers:

```text
FBattleHUDStatusView.RuntimeSequence
Gameplay uint64 -> Presentation int64 guard
FinalSnapshot RuntimeSequence sorting
StatusChanged visible playback participation
WorkingPresentationSnapshot status reducer
exact StatusId + RuntimeSequence identity
AmountBefore chain validation
create / increase / reduce / remove historical projection
stale RuntimeSequence mismatch -> FinalSnapshot collapse
amount mismatch -> FinalSnapshot collapse
Presentation mismatch does not mutate Gameplay
Presentation mismatch does not manufacture ResolutionFault
```

## Post-validation review notes

A follow-up A2D1-A2D3 source review found three defensive-hardening opportunities that do not invalidate the successful validation result:

```text
1. Status reducer payload validation does not currently validate SourcePresentationId.
2. Reducer validation relies on the producer for create DescriptionBefore == Empty and
   remove DescriptionAfter == Empty instead of re-validating those payload boundaries.
3. Frozen Status snapshots validate RuntimeSequence ordering/uniqueness, while duplicate
   StatusId rejection is enforced by the reducer rather than explicitly at freeze time.
```

These should be considered before A2D-5 combined acceptance and can be covered with focused malformed-history tests.

## Phase boundary

A2D-3 is closed at the C++/Automation level.

The next implementation slice is:

```text
A2D-4
FTerminalPresentationPayload
FResolutionFaultPresentationPayload
Victory / Defeat / ResolutionFault formal visible playback
terminal WorkingSnapshot reducer
terminal ViewModel transition timing
```

Blueprint integration and PIE smoke remain intentionally deferred until the complete A2D C++ presentation pipeline is ready for combined acceptance.
