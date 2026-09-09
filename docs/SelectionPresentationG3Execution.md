# Selection Presentation G3 Execution Record

Date: **2026-09-09**

Status:

```text
COMPLETE / VALIDATED / SEALED
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

## Validation evidence

Per `docs/ValidationExecutionPolicy.md`, G3 changes shared C++ playback infrastructure.
The required G3 gates were executed by the dedicated owner-only self-hosted workflow:

```text
.github/workflows/ue-selection-g3-tests.yml
Workflow run id: 34331464388
Validated commit: 07e1bf16ac315597fa53a4098a50969f5c19b83b
Runner: UE58-WIN
```

The validated commit contains the G3 implementation and the build repair
`a5c7e67c22ccf13c25ec91546a6d3f15e5002297`.

### Editor build

```text
PASS
SlayTheSpireDemoEditor Win64 Development
Result: Succeeded
Editor exit code: 0
```

The workflow performed a clean checkout/build and completed all 118 build actions,
including `SelectionPresentationG3TestTypes.cpp` and `SelectionPresentationG3Tests.cpp`,
then linked both Runtime and Tests Editor DLLs successfully.

### G3 focused Automation

```text
PASS
Prefix: SlayTheSpireDemo.SelectionPresentation.G3
Expected/discovered: 7 / 7
Failed: 0
Not run: 0
Editor exit code: 0
```

The workflow rejects any focused test whose individual state is not `Success`, so the
successful job conclusion plus exact 7-test discovery is the G3 focused evidence.

### Directly affected timeout regression

```text
PASS
SlayTheSpireDemo.Phase6UIA2D4.Playback.TerminalTimeout
Expected/discovered: 1 / 1
Failed: 0
Not run: 0
Editor exit code: 0
```

This covers the existing timeout/cancellation path directly affected by the common
playback-unit hardening.

### First build failure and repair history

The first UE 5.8 Editor build attempt failed before linking the Runtime module because
`DeckShuffledCountTrigger.cpp` called `IsValid(DeckShuffled->Deck)` while only the
forward declaration of `UDeckRuntime` was visible through `BattleEvent.h`. The compiler
could not convert `UDeckRuntime*` to `const UObject*` for `IsValid`.

Repair committed on `main`:

```text
a5c7e67c22ccf13c25ec91546a6d3f15e5002297
fix(build): include deck runtime in shuffled relic trigger
```

The successful workflow run above validates the post-repair build and supersedes the
first failed attempt.

### Test Unity-build experiment

After the successful G3 run, `SlayTheSpireDemoTests` was briefly switched to Unity
compilation to investigate build speed. Existing test translation units contain
namespace/helper-name collisions when amalgamated, so that experiment was reverted.
The current test module is restored to:

```text
bUseUnity = false;
```

This matches the compilation mode used by the successful G3 validation run and does
not change the G3 production implementation or its validated behavior.

G0/G1/G2 passing evidence remains sticky. No G3 failure implicated those sealed
Selection Presentation contracts.

No manual PIE Gate is required for G3 because production still plays SingleRecords
serially and no new visible Group animation, geometry, SelectionArea ownership, or
input behavior is enabled.

## Seal

G3 is now:

```text
COMPLETE / VALIDATED / SEALED
```

Do not rerun G3 gates unless later edits invalidate this playback-unit / exact
completion / scoped recovery contract or a concrete regression implicates it.

Next active slice:

```text
G4 — generic SingleRecord card transition engine
     + source resolver Hand | SelectionArea
     + migrate SingleRecord production transitions first
```

G4 must not switch production Selection ownership to durable SelectionArea (G5),
must not enable N-child parallel Group playback (G6), and must not introduce G8
cross-Resolution early input.
