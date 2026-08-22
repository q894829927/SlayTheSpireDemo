# Phase 6UI-A2D Current Code Review

Date: **2026-08-22**

Status: **REVIEW COMPLETE / NO BLOCKING DEFECT FOUND / HARDENING RECOMMENDED**.

Reviewed runtime source base:

```text
9efe59f6ef68cbc30d12bae4b403b0ceb39ea946
```

The documentation-only commits that record A2D-3 validation do not change the reviewed runtime/test implementation.

This review covers the currently implemented A2D slices:

```text
A2D-1 Status mutation truth / exact-instance actions
A2D-2 StatusChanged producer / frozen historical payload
A2D-3 Status WorkingSnapshot reducer / RuntimeSequence projection
```

A2D-4 terminal payload/reducer work and A2D-5 combined acceptance are not yet implemented and are treated as pending scope rather than defects in A2D-1 through A2D-3.

## Overall result

No blocker was found in the validated status path.

The principal contracts are coherent across the three slices:

```text
Gameplay StatusContainer owns mutation truth
-> Action freezes presentation-only value history
-> Record carries StatusId + RuntimeSequence exact identity
-> Controller reduces only immutable Record + WorkingSnapshot
-> mismatch collapses to immutable FinalSnapshot
-> Presentation failure never rolls Gameplay back
-> Presentation failure never manufactures Gameplay ResolutionFault
```

The current A2D-3 validation result remains authoritative:

```text
Phase6UIA2D3 focused Automation   PASS 4/4
Phase6R affected regression       PASS 88/88
UE5.8 Editor build                PASS through Phase6R prerequisite
```

## Finding 1 — invalid non-null Source can collapse into NAME_None

Severity: **Medium hardening**.

Path:

```text
Source/SlayTheSpireDemo/Presentation/StatusPresentationRecordBuilder.cpp
ResolveParticipantIds(...)
```

Current behavior uses `IsValid(Source)` to decide whether Source exists. Therefore these two states are currently treated the same:

```text
Source == nullptr
Source != nullptr but UObject is invalid/pending kill
```

The locked A2D contract distinguishes them:

```text
Source == nullptr
-> SourcePresentationId == NAME_None

Source supplied
-> it must be a valid current battle participant
-> it must resolve a non-empty authoritative PresentationId
```

A stale/destroyed non-null Source should not silently become an anonymous/system source.

Recommended hardening:

```cpp
if (Source != nullptr)
{
    if (!IsValid(Source)
        || !Battle->TryResolveCombatantPresentationId(Source, OutSourceId)
        || OutSourceId.IsNone())
    {
        return false;
    }
}
```

Recommended test:

```text
writer active
+ non-null invalid Source
+ committed Status mutation
-> unpublished Presentation Resolution invalidated
-> no partial Envelope
-> Gameplay commit remains authoritative
-> no Gameplay ResolutionFault
```

## Finding 2 — Controller Status validation does not validate SourcePresentationId

Severity: **Medium defensive validation**.

Path:

```text
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
ValidateStatusChangedPayload(...)
```

The reducer validates:

```text
TargetPresentationId
StatusId
RuntimeSequence
AmountBefore / AmountAfter
bCreated / bRemoved
Reason
```

but does not validate `SourcePresentationId`.

The producer currently generates trustworthy source IDs, so normal committed history is correct. The gap matters for malformed/synthetic/corrupted history: a Record can carry an impossible/fake SourcePresentationId, be offered to the Widget, and still pass the Status reducer because Source does not participate in snapshot mutation.

Recommended hardening is to validate that SourcePresentationId is either:

```text
NAME_None
or
one of the current snapshot participant PresentationIds
```

If future Status reasons require a non-null source, reason-specific validation can be added later without querying live Gameplay.

Recommended test:

```text
synthetic StatusChanged with fake SourcePresentationId
-> incremental history rejected/collapsed
-> FinalSnapshot remains authoritative
```

## Finding 3 — Description boundary semantics are producer-only

Severity: **Low/Medium defensive validation**.

Paths:

```text
Source/SlayTheSpireDemo/Presentation/StatusPresentationRecordBuilder.cpp
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
```

The producer correctly enforces:

```text
create -> DescriptionBefore must be Empty
remove -> DescriptionAfter must be Empty
```

The Controller's `ValidateStatusChangedPayload()` does not repeat these immutable payload checks. Therefore malformed history with otherwise valid identity/amount shape can pass reducer validation even if the create/remove description boundary is inconsistent.

Recommended hardening:

```text
Payload.bCreated && !Payload.DescriptionBefore.IsEmpty() -> reject
Payload.bRemoved && !Payload.DescriptionAfter.IsEmpty()   -> reject
```

Do not require every non-created `DescriptionBefore` to be non-empty; empty authored descriptions are valid.

Recommended tests:

```text
create with non-empty DescriptionBefore -> collapse
remove with non-empty DescriptionAfter  -> collapse
```

## Finding 4 — freeze/reducer uniqueness rules are slightly asymmetric

Severity: **Low invariant hardening**.

Paths:

```text
Source/SlayTheSpireDemo/Battle/BattleManagerPresentation.cpp
Source/SlayTheSpireDemo/Presentation/BattlePresentationController.cpp
```

Frozen snapshots enforce RuntimeSequence validity and strict sequence ordering. The reducer additionally rejects duplicate live `StatusId` values.

Normal Gameplay cannot create duplicate StatusId instances because `StatusContainer` merges same-ID applications, so this is not an observed runtime defect. However the authoritative freeze boundary should ideally establish every invariant later required by the reducer.

Recommended hardening:

```text
FreezeCombatant
-> reject duplicate StatusId
-> reject duplicate RuntimeSequence
-> sort RuntimeSequence ascending
```

The RuntimeSequence duplicate is already rejected by strict post-sort ordering; explicit StatusId uniqueness would make the freeze/reducer contracts symmetric.

Recommended test:

```text
synthetic/read-state duplicate StatusId
-> frozen Presentation baseline rejected
```

## Observation — compatibility ReduceStatusAction overload is a presentation footgun

Severity: **Low API hygiene**.

Path:

```text
Source/SlayTheSpireDemo/Actions/ReduceStatusAction.h/.cpp
Initialize(UStatusContainer*, UStatusInstance*, int32)
```

The compatibility overload intentionally sets `Battle = nullptr` and is documented for pre-A2D direct callers. If it is accidentally used in a writer-active queued Resolution, Gameplay can commit and the subsequent Status record build will invalidate Presentation because no authoritative BattleManager is available.

This is fail-soft and does not corrupt Gameplay, but it can unnecessarily discard the entire unpublished presentation batch.

Recommended cleanup before A2D-5:

```text
keep the overload only for explicit no-history/test use
or
add an early writer-active guard before mutation
or
remove it once legacy callers are migrated
```

## Confirmed strengths

The review specifically confirms the following implementation choices are sound:

```text
FStatusMutationResult stays Gameplay-only and synchronous
same-StatusId apply merges into the existing EffectiveDefinition
CandidateRuntimeSequence gaps are legal
stale exact Reduce/Remove never retarget a replacement instance
removed UStatusInstance is consumed synchronously only
StatusChanged stores value data, not live Status UObject references
DisplayName fallback matches FinalSnapshot freezing
RuntimeSequence conversion checks MAX_int64 before Presentation conversion
Status reducer does not query live Gameplay
Status reducer preserves sequence ordering
AmountBefore mismatch collapses to FinalSnapshot
Presentation mismatch does not create Gameplay ResolutionFault
terminal records remain recorder-terminal even before A2D-4 formal payload work
```

## A2D-4 boundary confirmed

Current Controller behavior still reflects the intentionally deferred terminal slice:

```text
ResolutionFault participates in the existing visible path
Victory / Defeat are not yet formal visible-playback records
terminal Records do not yet have the A2D-4 typed payload reducer
WorkingSnapshot terminal transition timing is not yet implemented
```

These should be implemented in A2D-4 rather than patched ad hoc into A2D-3.

## Recommended order before A2D-5

```text
1. Implement A2D-4 terminal payload + reducer contract.
2. Add SourcePresentationId defensive validation.
3. Add create/remove description-boundary reducer validation.
4. Align frozen StatusId uniqueness with reducer invariants.
5. Decide whether to remove/guard the legacy ReduceStatusAction overload.
6. Run A2D-4 focused gate.
7. Run updated Phase6R aggregate.
8. Execute A2D-5 combined status + card + turn-cycle + terminal acceptance.
```

None of the hardening findings above requires reopening the already validated A2D-3 core behavior unless a new focused test exposes a concrete regression.
