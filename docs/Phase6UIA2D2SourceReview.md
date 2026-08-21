# Phase 6UI-A2D2 Static Source Review

Date: **2026-08-22**

Status: **STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

A2D-2 implements the locked `StatusChanged + frozen DescriptionBefore/After` committed-presentation slice. This document records source/static review only; it does not claim UnrealHeaderTool, MSVC, Unreal Editor, Automation, Blueprint or PIE execution.

## Implemented scope

```text
EBattlePresentationRecordType::StatusChanged (append-only)
FStatusChangedPresentationPayload
StatusPresentationRecordBuilder
ApplyStatusAction producer
ReduceStatusAction producer
RemoveStatusAction producer
Source/Target PresentationId resolution
uint64 RuntimeSequence -> int64 safety validation
EffectiveDefinition visual freezing
DescriptionBefore / DescriptionAfter capture
invalid-history fail-soft behavior
```

`FStatusChangedPresentationPayload` freezes:

```text
SourcePresentationId
TargetPresentationId
StatusId
RuntimeSequence
AmountBefore / AmountAfter
bCreated / bRemoved
Reason
DisplayName
DescriptionBefore / DescriptionAfter
bUseAtlasIcon
UVOffset / UVScale
TrimOffset / TrimScale
```

No live `UStatusInstance` / `UStatusData` reference is stored in the Record.

## Before/after capture contract

Apply:

```text
read-only pre-lookup by StatusId
→ freeze DescriptionBefore from the existing runtime instance, or Empty for create
→ ApplyStatusCommit
→ validate the committed result against the pre-mutation identity
→ freeze DescriptionAfter from Result.EffectiveInstance
→ derive Applied / Increased from the committed result
→ append StatusChanged
```

Reduce/remove:

```text
capture exact ExpectedInstance + DescriptionBefore
→ exact-instance Gameplay mutation
→ validate Result.EffectiveInstance is the same synchronous runtime identity
→ freeze DescriptionAfter when retained, otherwise Empty
→ append StatusChanged
```

The static review found and corrected one compile-level issue during implementation: `ApplyStatusAction` initially attempted to call private `FindMutableStatusById()`. It now uses the public read-only `FindStatusById()` API for pre-mutation text capture.

## Identity / failure rules

With an active Writer, a committed Status record requires:

```text
Battle is valid
Target is valid and resolves a non-empty PresentationId
Source == nullptr -> NAME_None
Source valid -> resolves a non-empty PresentationId
StatusId != None
RuntimeSequence > 0
RuntimeSequence <= MAX_int64
EffectiveInstance / EffectiveDefinition are valid and consistent
pre-mutation exact identity matches the committed mutation for update/remove paths
Reason matches the actual committed mutation
```

If Gameplay already committed but one of these Presentation invariants fails:

```text
invalidate current unpublished Presentation Resolution
publish no partial Envelope
keep Gameplay commit
never manufacture Gameplay ResolutionFault
```

No-op and invalid Gameplay mutations emit no `StatusChanged` record.

## Reason mapping

```text
create                         -> Applied
committed merge increase       -> Increased
ordinary exact reduction       -> Reduced
turn-end exact reduction       -> TurnEndDecay
explicit exact remove          -> Removed
```

A `TurnEndDecay` that reaches zero keeps `Reason=TurnEndDecay` while setting `bRemoved=true`.

## Frozen definition rule

Same-`StatusId` merge with a different incoming `UStatusData*` freezes presentation data from the existing runtime instance's `EffectiveDefinition`, not from the incoming request definition.

This applies to DisplayName, dynamic Description and atlas metadata.

## Focused Automation authored

Exactly four top-level tests are present under:

```text
SlayTheSpireDemo.Phase6UIA2D2.Record.ApplyFreeze
SlayTheSpireDemo.Phase6UIA2D2.Record.ReduceAndRemoveFreeze
SlayTheSpireDemo.Phase6UIA2D2.Record.TurnEndDecay
SlayTheSpireDemo.Phase6UIA2D2.Failure.InvalidHistoryDoesNotAffectGameplay
```

Coverage includes:

```text
Applied / Increased records
pre/post dynamic Amount descriptions
existing EffectiveDefinition on same-StatusId merge
atlas metadata freezing
partial Reduced record
explicit Removed record
nullable Source -> NAME_None
TurnEndDecay removal preserving cause
RuntimeSequence positive / int64 boundary
missing authoritative Battle context with active Writer -> invalid history
Presentation invalidation does not roll back Gameplay
no partial Envelope
no Gameplay ResolutionFault from Presentation failure
```

## CI gates

Dedicated manual workflow:

```text
.github/workflows/ue-phase6uia2d2-tests.yml
Prefix: SlayTheSpireDemo.Phase6UIA2D2
ExpectedCount: 4
```

Aggregate Phase6R now includes A2D-2:

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
------------------
Total           84
```

## Explicitly out of A2D-2

The following remain for later slices:

```text
A2D-3 FBattleHUDStatusView.RuntimeSequence
A2D-3 FinalSnapshot RuntimeSequence sorting
A2D-3 WorkingSnapshot Status reducer
A2D-4 Victory / Defeat / ResolutionFault formal payload/playback
A2D-5 combined acceptance
Blueprint integration
PIE smoke
```

## Validation status

```text
A2D-2 StatusChanged types            IMPLEMENTED
A2D-2 Apply/Reduce/Remove producers  IMPLEMENTED
Frozen Before/After descriptions     IMPLEMENTED
EffectiveDefinition freezing         IMPLEMENTED
RuntimeSequence conversion guard     IMPLEMENTED
Focused Automation source            AUTHORED: 4 tests
Focused GitHub Actions workflow      CONFIGURED: 4 expected
Phase6R aggregate                    CONFIGURED: 84 expected
Static compile/UHT review            COMPLETE
UE5.8 Editor build                   NOT RUN for A2D-2
Phase6UIA2D2 Automation              NOT RUN
84-test affected regression          NOT RUN
```

Do not mark A2D-2 validated until UE5.8 build, focused 4/4 and affected 84/84 regression execute successfully.
