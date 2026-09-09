# Selection Presentation G3 Execution Record

Date: **2026-09-09**

Status:

```text
IMPLEMENTED IN SOURCE /
BUILD NOT RUN / AUTOMATION NOT RUN / NO PASS CLAIM
```

## Scope

G3 only: Base Widget / Controller playback-unit hardening, exact completion identity,
and scoped Presentation recovery.

Explicitly not included:

```text
G4 generic card transition engine
G5 production SelectionArea ownership source switch
G6 visible/parallel multi-child Group playback
G7 legacy-path deletion
G8 cross-Resolution early input / pipelining
```

G2 semantic Group discovery remains sealed and unchanged. G3 does not call semantic
Group candidates from production playback and therefore does not make grouped
Selection movement visible.

## Implemented contract

### One playback-unit token

`FPresentationPlaybackToken` now carries the exact unit identity:

```text
BattleId
ResolutionId
PresentationSequence
LocalPlaybackGeneration
UnitKind = SingleRecord | Group
GroupId (0 for SingleRecord, exact positive GroupId for Group)
```

Token equality includes every field. Controller allocates a fresh local generation
for every offered production SingleRecord unit. A stale Group token therefore cannot
complete/cancel a newer SingleRecord, and a stale SingleRecord token cannot complete
or cancel a newer Group even if they share Battle/Resolution/leader sequence.

### Base Widget single owner

`UBattleHUDWidgetBase` has one tracked playback-unit owner, never independent
Record/Group owners.

SingleRecord keeps the existing Blueprint `BeginPresentationRecordPlayback` and
`CancelPresentationRecordPlayback` contract. G3 adds dormant Group Begin/Cancel hooks
as **C++-only virtuals**; no new Group Blueprint asset contract is introduced before
G6 authorizes visible Group playback.

Both unit kinds share:

```text
track exact token before concrete Begin
Begin false -> clear tracked owner without fake Cancel/Finish
Notify -> CoreTicker deferred forwarding
stale/duplicate Notify -> rejected before Controller forwarding
exact-token Cancel only
replacement -> cancel previous exact unit before new ownership
```

Production Controller continues to call only `PlayPresentationRecord()` in G3.

### Recovery scopes

Controller recovery is split into distinct paths:

```text
ActivePlaybackUnit
    exact cancel of the current visual token

ActiveEnvelope
    cancel exact active visual
    -> apply ActiveEnvelope.FinalSnapshot
    -> mark exact (BattleId, ResolutionId) completion
    -> discard only ActiveEnvelope
    -> preserve/start later queued Envelopes

EntireBacklog
    cancel exact active visual
    -> apply newest applicable FinalSnapshot
    -> mark each discarded Envelope's exact Resolution completion
    -> clear active + queued backlog
```

ActiveEnvelope recovery no longer reuses the old whole-backlog collapse behavior.
A future Group timeout therefore cannot silently delete later committed Envelopes.

### Exact recorded completion source

Controller uses the existing G0-C ViewModel API:

```text
MarkPresentationResolutionCompleted(BattleId, ResolutionId)
```

The API records exact Resolution identities; it is not an ordinal `>=` watermark.

Normal and recovery ordering is intentionally:

```text
exact visual cleanup (when applicable)
-> applicable FinalSnapshot displayed
-> exact Resolution completion published
-> ownership reconciliation triggered by the exact completion fact
```

Normal zero-record Envelopes use the same `CompleteActiveEnvelope()` source and
therefore publish exact completion after their FinalSnapshot is displayed.

### Direct / unavailable / battle replacement

Direct-baseline transition cancels exact in-flight visual work, applies the latest
frozen baseline, then publishes exact completion for discarded same-battle recorded
Envelopes before clearing Controller backlog.

Presentation-unavailable transition does not pretend abandoned Envelopes completed
normally. It cancels Controller playback, applies the available frozen baseline, and
uses `UBattleHUDViewModel::EnterPresentationUnavailable()` whose explicit semantics
reset transient card Presentation ownership.

Battle replacement similarly invalidates old playback by exact cancellation /
generation change; applying the new BattleId snapshot reconciles old-battle ownership
entries away.

No Presentation failure/timeout/skip path requests Gameplay `ResolutionFault`.

## Focused Automation source

New G3-specific fixture avoids modifying historical Phase6 shared fixture classes.

```text
SlayTheSpireDemo.SelectionPresentation.G3.PlaybackUnitTracking
SlayTheSpireDemo.SelectionPresentation.G3.CrossKindStaleAndDeferred
SlayTheSpireDemo.SelectionPresentation.G3.ActiveEnvelopeRecovery
SlayTheSpireDemo.SelectionPresentation.G3.GroupTimeoutScope
SlayTheSpireDemo.SelectionPresentation.G3.ZeroRecordExactCompletion
SlayTheSpireDemo.SelectionPresentation.G3.RecoveryExactWatermark
SlayTheSpireDemo.SelectionPresentation.G3.GlobalSkipEntireBacklog
```

The dormant Group timeout test uses a test-only Controller rebind to exercise the
common Group token/cancel/timeout boundary without changing production playback
selection. It verifies timeout reconciles the whole ActiveEnvelope, not the leader
Record, and that a later queued Envelope resumes normally.

Exact watermark tests prove:

```text
larger unrelated ResolutionId != completion of the target Resolution
ActiveEnvelope recovery completes only its exact recorded ownership watermark
EntireBacklog Skip intentionally completes every discarded exact Resolution
```

## Required validation

Per `docs/ValidationExecutionPolicy.md`, G3 changes shared C++ playback infrastructure.
Required evidence before any PASS/seal claim:

```text
[ ] UE 5.8 SlayTheSpireDemoEditor Win64 Development build
[ ] SlayTheSpireDemo.SelectionPresentation.G3 focused Automation
[ ] directly affected existing timeout/exact-token playback regression coverage
```

At minimum the existing terminal timeout path is directly affected by the common
timeout/cancellation rewrite:

```text
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout
```

A dedicated focused runner is now available at:

```text
.github/workflows/ue-selection-g3-tests.yml
```

It is owner-only, `workflow_dispatch`-only, and restricted to `main`. One run performs
one UE 5.8 Editor build, the seven `SlayTheSpireDemo.SelectionPresentation.G3` tests,
and the exact `SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout` regression.
The workflow existing in source is not validation evidence by itself; an actual
successful run is still required.

G0/G1/G2 passing evidence remains sticky unless the build/test failure implicates a
sealed contract. G3 does not modify G2 semantic-preflight source or tests.

No manual PIE Gate is required for G3 because production still plays SingleRecords
serially and no new visible Group animation, geometry, SelectionArea ownership, or
input behavior is enabled.

## Current checkpoint

Source implementation and the focused validation workflow are complete, but no Editor
build or Automation run has been executed for this G3 head yet. The current GitHub
connector can inspect and rerun existing workflow runs but cannot dispatch a new
`workflow_dispatch` run, so execution of `.github/workflows/ue-selection-g3-tests.yml`
is currently **USER ACTION REQUIRED**. Do not mark G3
`COMPLETE / VALIDATED / SEALED` until the required successful run evidence is supplied.