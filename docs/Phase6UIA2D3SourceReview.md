# Phase 6UI-A2D3 Static Source Review

Date: **2026-08-22**

Status: **STATIC REVIEW COMPLETE / UE5.8 VALIDATION PENDING**.

A2D-3 implements the locked Status historical projection slice: `FBattleHUDStatusView.RuntimeSequence`, deterministic frozen Status ordering, `StatusChanged` WorkingPresentationSnapshot reduction, and mismatch collapse to the immutable `Envelope.FinalSnapshot`.

This document records source/static review only. It does not claim UnrealHeaderTool, MSVC, Unreal Editor, Automation, Blueprint, or PIE execution.

## Implemented scope

```text
FBattleHUDStatusView.RuntimeSequence
Gameplay uint64 -> Presentation int64 freeze guard
FinalSnapshot Status RuntimeSequence sorting
strict frozen RuntimeSequence uniqueness/order
StatusChanged visible-playback participation
WorkingPresentationSnapshot Status reducer
exact StatusId + RuntimeSequence matching
AmountBefore chain validation
create / increase / reduce / remove projection
reducer mismatch -> FinalSnapshot collapse
```

A2D-3 does not query live Gameplay from the Controller reducer.

## Frozen snapshot identity and ordering

`FStatusReadView` already carries Gameplay `uint64 RuntimeSequence`. A2D-3 freezes it into:

```cpp
FBattleHUDStatusView::RuntimeSequence // int64
```

The freeze path rejects a Status if:

```text
StatusId == NAME_None
Amount <= 0
RuntimeSequence == 0
RuntimeSequence > MAX_int64
```

After freezing, each combatant Status array is explicitly sorted by:

```text
RuntimeSequence ascending
```

The freeze path then verifies strict ordering. Duplicate/non-positive Presentation runtime identities cannot become an authoritative frozen baseline.

This removes the former dependency on `UStatusContainer` insertion order.

## WorkingSnapshot reducer

For every `StatusChanged` record, the Controller first resolves the target combatant only from:

```text
Record.StatusChanged.TargetPresentationId
```

The concrete Status identity is:

```text
StatusId + RuntimeSequence
```

within that target combatant.

Before applying a Status change, the reducer validates the existing Status projection:

```text
StatusId non-empty
RuntimeSequence > 0
Amount > 0
RuntimeSequence strictly ascending
no duplicate live StatusId
```

### Create

`Applied` requires:

```text
bCreated == true
bRemoved == false
AmountBefore == 0
AmountAfter > 0
```

The reducer rejects a create if either the live StatusId or the RuntimeSequence already exists. It builds `FBattleHUDStatusView` entirely from the frozen payload and inserts it at the RuntimeSequence-sorted position.

### Increase / reduce

For an existing Status, the reducer requires the exact identity and:

```text
Current.Amount == Record.AmountBefore
```

It then applies:

```text
AmountAfter
DescriptionAfter
frozen DisplayName
frozen atlas metadata
```

The existing RuntimeSequence remains unchanged.

### Remove

Removal requires the same exact identity and AmountBefore check. Only that exact array entry is removed.

A stale `Weak#10` record therefore cannot modify or remove a replacement `Weak#50`.

## Reason / structure validation

Reducer-side validation mirrors the committed producer semantics:

```text
Applied
  create 0 -> positive

Increased
  positive -> larger positive

Reduced / TurnEndDecay
  positive -> smaller non-negative
  bRemoved exactly matches AmountAfter == 0

Removed
  explicit positive -> 0
  bRemoved == true
```

Malformed historical payloads are not partially applied.

## Reducer mismatch behavior

Any Status historical mismatch returns reducer failure, including:

```text
unknown TargetPresentationId
invalid StatusId / RuntimeSequence
unsorted/invalid current Status projection
create duplicates StatusId or RuntimeSequence
exact identity missing
AmountBefore mismatch
invalid Reason/amount/membership shape
```

The existing Controller path then performs:

```text
stop incremental playback
-> CollapseToEnvelope
-> apply immutable Envelope.FinalSnapshot
-> advance playback generation
```

This is Presentation recovery only. It does not mutate Gameplay and does not manufacture `ResolutionFault`.

## Status projection boundary

The A2D-3 reducer owns only Status projection values:

```text
Combatant.Statuses identity/order
Amount
Description
DisplayName
atlas metadata
```

It deliberately does not recompute from live Gameplay:

```text
card dynamic descriptions
enemy intent derived damage
playability / legal-target bindings
other indirectly status-dependent projections
```

Those remain exact at Envelope completion through `FinalSnapshot` reconciliation.

## A2D-2 integration hardening found during A2D-3

Static review of reducer/final-snapshot equivalence exposed two producer consistency details and they were corrected before A2D-3 validation:

1. A legitimately empty authored Status description is allowed. Producer boundary checks now require only creation-Before empty and removal-After empty; they do not treat every non-created empty `DescriptionBefore` as corrupt history.
2. Frozen `StatusChanged.DisplayName` now uses the same fallback as FinalSnapshot freezing:

```text
Definition.DisplayName empty
-> FText::FromName(StatusId)
```

The reducer and FinalSnapshot therefore use the same visible metadata convention.

## Visible playback boundary

`StatusChanged` now participates in the generic Controller visible-playback path.

```text
Blueprint accepts -> wait for normal token callback
Blueprint returns false / no Widget -> native immediate completion
Timeout -> existing fail-safe completion
```

A2D-3 adds no Blueprint animation. Blueprint/PIE integration remains deferred.

## Focused Automation authored

Exactly four top-level tests exist under:

```text
SlayTheSpireDemo.Phase6UIA2D3.Snapshot.RuntimeSequenceSorting
SlayTheSpireDemo.Phase6UIA2D3.Playback.StatusLifecycleReducer
SlayTheSpireDemo.Phase6UIA2D3.Safety.StaleRuntimeSequenceCollapses
SlayTheSpireDemo.Phase6UIA2D3.Safety.AmountMismatchCollapses
```

Coverage includes:

```text
explicit 30 / 10 / 20 runtime identities -> frozen 10 / 20 / 30
RuntimeSequence carried into FBattleHUDStatusView
create reducer insertion order
Applied / Increased / Reduced / Removed lifecycle
DescriptionAfter intermediate projection
DisplayName fallback consistency
exact RuntimeSequence preservation on update
exact removal
stale old RuntimeSequence cannot remove replacement instance
AmountBefore mismatch collapses to FinalSnapshot
Presentation collapse does not mutate Gameplay
Presentation mismatch does not manufacture Gameplay fault
```

## CI gates

Dedicated manual workflow:

```text
.github/workflows/ue-phase6uia2d3-tests.yml
Prefix: SlayTheSpireDemo.Phase6UIA2D3
ExpectedCount: 4
```

Aggregate Phase6R now includes A2D-3:

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

## Static compile / UHT review

The review checked the touched reflected types and main C++ call boundaries:

```text
FBattleHUDStatusView int64 BlueprintReadOnly field
FStatusReadView uint64 -> int64 checked conversion
TArray Status sorting/insertion
BattlePresentationController Status reducer helpers
existing FPresentationRecord StatusChanged payload
existing UPhase6UIA2APlaybackWidget async test transport
BattleActionQueue batch signature
BattlePresentationRecorder HasActiveResolution public access
```

No definite C++/UHT blocker was found by source inspection. UE5.8 build remains authoritative.

## Explicitly out of A2D-3

```text
A2D-4 FTerminalPresentationPayload
A2D-4 FResolutionFaultPresentationPayload
A2D-4 Victory / Defeat / ResolutionFault formal visible playback
A2D-4 terminal reducer / terminal ViewModel timing
A2D-5 combined acceptance matrix
unified Blueprint integration
PIE smoke
```

## Validation status

```text
FBattleHUDStatusView.RuntimeSequence       IMPLEMENTED
FinalSnapshot RuntimeSequence sorting      IMPLEMENTED
StatusChanged WorkingSnapshot reducer      IMPLEMENTED
exact identity + AmountBefore validation   IMPLEMENTED
mismatch -> FinalSnapshot collapse         IMPLEMENTED
Focused Automation source                  AUTHORED: 4 tests
Focused GitHub Actions workflow            CONFIGURED: 4 expected
Phase6R aggregate                          CONFIGURED: 88 expected
Static compile/UHT review                  COMPLETE
UE5.8 Editor build                         NOT RUN for A2D-3
Phase6UIA2D3 Automation                    NOT RUN
88-test affected regression                NOT RUN
```

Do not mark A2D-3 validated until UE5.8 Editor build, focused 4/4, and affected Phase6R 88/88 execute successfully.
