# Phase 6UI-A2D4 Validation

Date: **2026-08-22**

Status: **FOCUSED VALIDATED / FULL REGRESSION PENDING**.

## Validated incoming baseline

Before A2D-4 implementation:

```text
UE5.8 Editor build                 PASS
SlayTheSpireDemo.Phase6UIA2D1     PASS 3/3
SlayTheSpireDemo.Phase6UIA2D2     PASS 4/4
SlayTheSpireDemo.Phase6UIA2D3     PASS 4/4
Phase6R aggregate                 PASS 88/88
```

Those results validate the incoming A2D-1 through A2D-3 baseline only. A2D-4 changes runtime Presentation code, compatibility tests and CI counts, so a fresh full regression run is still required before promotion to A2D-5.

## Current A2D-4 implementation

Implemented terminal contract:

```text
FTerminalPresentationPayload
FResolutionFaultPresentationPayload
Victory / Defeat typed producer identities
ResolutionFault typed diagnostics
legacy root fault fields removed
terminal Envelope shape preflight
Victory / Defeat / ResolutionFault visible playback
pre-Blueprint terminal reducer preflight
real WorkingSnapshot terminal transition after completion
native false fallback
timeout
skip / stale callback isolation
FinalSnapshot exact reconciliation
```

## Focused gate

Workflow:

```text
.github/workflows/ue-phase6uia2d4-tests.yml
```

Expected:

```text
Prefix: SlayTheSpireDemo.Phase6UIA2D4
Tests:  6/6
Failed: 0
NotRun: 0
EditorExit: 0
```

Expected top-level tests:

```text
SlayTheSpireDemo.Phase6UIA2D4.Producer.VictoryPayload
SlayTheSpireDemo.Phase6UIA2D4.Producer.DefeatPayload
SlayTheSpireDemo.Phase6UIA2D4.Producer.ResolutionFaultPayload
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalCompletionTiming
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout
SlayTheSpireDemo.Phase6UIA2D4.Safety.PreflightFallbackSkip
```

Current result after the timeout-test include fix (`f0d03895`):

```text
UE5.8 Editor build    PASS
A2D4 focused 6/6      PASS
Failed                 0
NotRun                 0
EditorExit             0
```

This focused run validates that the A2D-4 source compiles under UE5.8 and that all six terminal producer/playback/safety tests pass on the hardened implementation.

## Regression gate

Updated Phase6R expected counts:

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
Phase6UIA2D4     6
------------------
Total           94
```

Required aggregate result:

```text
Phase6R 94/94
Failed  0
NotRun  0
EditorExit 0
Shipping exclusion PASS
```

Current result:

```text
Phase6R 94/94       PENDING
Shipping exclusion  PENDING
```

## Promotion rule

Do not mark A2D-4 complete or begin A2D-5 acceptance work based only on the focused gate.

Promotion requires all of:

```text
UE5.8 Editor Development build PASS      [PASS]
A2D4 focused 6/6 PASS                    [PASS]
A2D1 3/3 PASS                            [pending fresh Phase6R]
A2D2 4/4 PASS                            [pending fresh Phase6R]
A2D3 4/4 PASS                            [pending fresh Phase6R]
Phase6R 94/94 PASS                       [PENDING]
Shipping exclusion PASS                  [PENDING]
```

After those pass, update this document and `Phase6UIA2D4SourceReview.md` to:

```text
VALIDATED / READY FOR A2D-5
```
