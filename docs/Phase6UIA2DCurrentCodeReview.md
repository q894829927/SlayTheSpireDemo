# Phase 6UI-A2D Current Code Review

Date: **2026-08-22**

Status: **REVIEW FINDINGS FIXED / STATIC RE-REVIEW COMPLETE / UE5.8 REVALIDATION PENDING**.

Reviewed hardened source head includes:

```text
9ba552e1  invalid non-null Source rejection
adc82508  stale identity validation before NoOp
6c0b2d47  duplicate StatusId freeze rejection
82b36508  pre-playback Status preflight + bootstrap hardening
6114c8e8  A2D1 regression coverage
9fd6701a  A2D2 invalid-source coverage
79dff285  A2D3 hardened playback/boundary coverage
```

This review covers the implemented A2D-1 through A2D-3 status path. A2D-4 terminal payload/reducer work and A2D-5 combined acceptance remain separate pending scope.

## Overall result

The previously reported hardening findings have been resolved in source and covered by focused Automation source changes. No new blocking defect was found by static inspection.

The current architecture remains:

```text
Gameplay StatusContainer owns mutation truth
-> Action freezes presentation-only value history
-> Record carries StatusId + RuntimeSequence exact identity
-> Controller prevalidates malformed Status history before Blueprint
-> valid playback completes
-> Controller commits to real WorkingSnapshot
-> mismatch collapses to immutable FinalSnapshot
-> Presentation failure never rolls Gameplay back
-> Presentation failure never manufactures Gameplay ResolutionFault
```

The previous 4/4 and 88/88 results predate these source changes. Current hardened head requires revalidation.

## Resolved 1 — supplied invalid Source no longer becomes anonymous

Path:

```text
Source/SlayTheSpireDemo/Presentation/StatusPresentationRecordBuilder.cpp
```

Current rule:

```text
Source == nullptr
-> SourcePresentationId == NAME_None

Source != nullptr
-> Source must be IsValid
-> must resolve through BattleManager
-> resolved PresentationId must be non-empty
```

A supplied invalid Source now invalidates the unpublished Presentation Resolution rather than being silently frozen as a system/anonymous source.

## Resolved 2 — Controller validates SourcePresentationId

Path:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
```

Status reducer validation now accepts only:

```text
NAME_None
or WorkingSnapshot.Player.PresentationId
or WorkingSnapshot.Enemy.PresentationId
```

A fake/impossible historical source therefore fails incremental history validation without querying live Gameplay.

## Resolved 3 — description structural boundaries are double-checked

Controller now mirrors the producer's structural rules:

```text
bCreated && DescriptionBefore non-empty -> reject
bRemoved && DescriptionAfter non-empty  -> reject
```

This deliberately does not reject an empty authored description on ordinary retained statuses.

## Resolved 4 — freeze/reducer StatusId uniqueness is symmetric

Path:

```text
Source/SlayTheSpireDemo/Battle/BattleManagerPresentation.cpp
```

`FreezeCombatant` now maintains a `TSet<FName>` and rejects duplicate StatusId at the authoritative frozen-baseline boundary. RuntimeSequence strict ordering/uniqueness remains enforced after sorting.

## Resolved 5 — Status Record is validated before Blueprint playback

Path:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
StartNextRecord
```

For `StatusChanged`, Controller now performs a reducer preflight on a copy:

```text
PreflightSnapshot = WorkingPresentationSnapshot
ApplyStatusChangedRecord(PreflightSnapshot, Record.StatusChanged)
```

If preflight fails:

```text
no PlaybackToken ownership is handed to Blueprint
PlayPresentationRecord is not called
Controller collapses directly to Envelope.FinalSnapshot
```

If preflight succeeds, the copy is discarded. The real `WorkingPresentationSnapshot` remains unchanged while animation is in progress and is committed only from `CompleteActiveRecord` after Blueprint callback, native fallback, or timeout.

This preserves the historical timing contract while preventing visibly playing a known-corrupt record.

## Resolved 6 — structurally invalid stale instance is Invalid, not NoOp

Path:

```text
Source/SlayTheSpireDemo/Status/StatusContainer.cpp
ReduceStatusCommit
RemoveStatusCommit
```

The methods now validate complete historical identity before checking exact Container membership:

```text
Owner matches
Definition valid
StatusId non-empty
RuntimeSequence > 0
Amount > 0
```

Only a structurally valid old instance missing from the Container is classified as `NoOp`. Malformed parameters remain `Invalid`.

## Resolved 7 — Controller bootstrap repairs stale ViewModel before watermarking

Path:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
Initialize
```

The implementation already applied `ApplyDisplayedSnapshot(Baseline, false)` before raising watermarks; the hardened contract now additionally takes Presentation display ownership explicitly when committed Presentation is active and has regression coverage for a deliberately stale ViewModel.

Order is now explicit:

```text
bind Controller
-> take Presentation display ownership
-> fetch frozen baseline
-> apply baseline to ViewModel / Controller snapshots
-> advance LastQueuedResolutionId / LastCompletedResolutionId
-> refresh bindings if caught up
```

This removes reliance on Presenter initialization order for correctness.

## Regression coverage

No new top-level test prefix was added, so CI expected counts remain stable:

```text
A2D1  3
A2D2  4
A2D3  4
Phase6R total 88
```

Coverage now additionally proves:

```text
invalid stale identity != valid stale NoOp
invalid supplied Source invalidates history only
fake historical SourcePresentationId cannot reach Blueprint
malformed create/remove description boundary cannot reach Blueprint
duplicate frozen StatusId is rejected
stale RuntimeSequence cannot reach Blueprint
AmountBefore mismatch cannot reach Blueprint
valid Status still advances only after playback completion
Controller bootstrap repairs stale ViewModel state
```

## Remaining observation — legacy ReduceStatusAction overload

The compatibility overload:

```text
Initialize(UStatusContainer*, UStatusInstance*, int32)
```

still intentionally lacks authoritative Battle context. It remains useful for explicit no-history compatibility tests but is a low-severity API footgun if used inside a writer-active formal Resolution. This was not one of the seven requested defects and has not been removed in this hardening pass.

A2D-5 should either remove it after migration or explicitly restrict it to no-history/test use.

## Static re-review result

No high-confidence C++/UHT blocker was found in the requested fixes by source inspection. The following still require the authoritative UE5.8 run:

```text
Editor compile/UHT
A2D1 3/3
A2D2 4/4
A2D3 4/4
Phase6R 88/88
```

Until those pass, the hardened current head is **REVALIDATION PENDING** rather than validated.
