# Phase 6UI-A2D3 Static Source Review

Date: **2026-08-22**

Status: **VALIDATED / READY FOR A2D-4**.

A2D-3 implements the locked Status historical projection slice: `FBattleHUDStatusView.RuntimeSequence`, deterministic frozen Status ordering, `StatusChanged` WorkingPresentationSnapshot reduction, and mismatch collapse to the immutable `Envelope.FinalSnapshot`.

The original static review is now supplemented by the user-reported UE5.8 validation result: focused A2D-3 Automation **4/4 PASS** and affected Phase6R regression **88/88 PASS**. The Phase6R workflow builds `SlayTheSpireDemoEditor Win64 Development` before running those tests, so the Editor-build prerequisite is also satisfied.

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

`FStatusReadView` carries Gameplay `uint64 RuntimeSequence`; A2D-3 freezes it into `FBattleHUDStatusView::RuntimeSequence` (`int64`). The freeze path rejects empty Status IDs, non-positive amounts, zero runtime identities, and identities above `MAX_int64`.

Each combatant Status array is sorted by `RuntimeSequence ascending`, and strict ordering is verified after sorting. This removes the former dependency on `UStatusContainer` insertion order.

## WorkingSnapshot reducer

For each `StatusChanged` record, the Controller resolves the target combatant only from `TargetPresentationId`. Concrete Status identity is:

```text
StatusId + RuntimeSequence
```

Before applying a mutation the reducer validates the current Status projection:

```text
StatusId non-empty
RuntimeSequence > 0
Amount > 0
RuntimeSequence strictly ascending
no duplicate live StatusId
```

Create rejects duplicate StatusId or RuntimeSequence and inserts the new frozen view at the RuntimeSequence-sorted location.

Increase/reduce requires the exact identity and `Current.Amount == Record.AmountBefore`, then applies `AmountAfter`, `DescriptionAfter`, DisplayName and atlas metadata from the immutable Record.

Remove requires the same exact identity and AmountBefore check and deletes only that exact runtime instance. A stale `Weak#10` record therefore cannot mutate or remove a replacement `Weak#50`.

## Reason / structure validation

Reducer-side validation mirrors producer semantics:

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

## Reducer mismatch behavior

Historical mismatches such as an unknown target, invalid Status identity, invalid current projection, duplicate create identity, missing exact identity, AmountBefore mismatch, or invalid reason/amount shape cause incremental playback to stop and collapse to the immutable `Envelope.FinalSnapshot`.

This is Presentation-only recovery: Gameplay is not mutated and no Gameplay `ResolutionFault` is manufactured.

## Status projection boundary

A2D-3 reduces only Status-owned projection values:

```text
Combatant.Statuses identity/order
Amount
Description
DisplayName
atlas metadata
```

It intentionally does not recalculate live card descriptions, enemy intent damage, playability, legal-target bindings, or other indirectly Status-dependent projections. Those are reconciled exactly at Envelope completion through `FinalSnapshot`.

## A2D-2 integration hardening found during A2D-3

A2D-3 review corrected two producer consistency details:

1. Legitimately empty authored Status descriptions are allowed; only creation-Before and removal-After boundary emptiness is required.
2. Frozen `StatusChanged.DisplayName` uses the same fallback as FinalSnapshot freezing: empty authored DisplayName falls back to `FText::FromName(StatusId)`.

## Visible playback boundary

`StatusChanged` participates in the generic Controller playback path:

```text
Blueprint accepts -> wait for token callback
Blueprint false / no Widget -> native immediate completion
Timeout -> existing fail-safe completion
```

A2D-3 itself adds no Blueprint animation. Blueprint/PIE integration remains deferred.

## Focused Automation

Exactly four top-level tests exist:

```text
SlayTheSpireDemo.Phase6UIA2D3.Snapshot.RuntimeSequenceSorting
SlayTheSpireDemo.Phase6UIA2D3.Playback.StatusLifecycleReducer
SlayTheSpireDemo.Phase6UIA2D3.Safety.StaleRuntimeSequenceCollapses
SlayTheSpireDemo.Phase6UIA2D3.Safety.AmountMismatchCollapses
```

Coverage includes sorting, runtime identity freezing, create/update/reduce/remove lifecycle, DescriptionAfter projection, DisplayName fallback, exact RuntimeSequence preservation, stale identity collapse, AmountBefore mismatch collapse, Gameplay non-mutation, and no synthetic Gameplay fault.

Reported result: **PASS (4/4)**.

## CI / regression gate

Dedicated workflow:

```text
.github/workflows/ue-phase6uia2d3-tests.yml
Prefix: SlayTheSpireDemo.Phase6UIA2D3
ExpectedCount: 4
```

Aggregate Phase6R includes:

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

Phase6R builds `SlayTheSpireDemoEditor Win64 Development` using the configured UE5.8 toolchain before running the prefixes, so the successful regression run also records the required Editor build as passed.

## Post-validation A2D review notes

A follow-up A2D1-A2D3 code review found three defensive-hardening opportunities. They do not invalidate the successful 4/4 and 88/88 validation:

```text
1. Controller Status payload validation does not currently validate SourcePresentationId.
2. Controller reducer relies on producer validation for create DescriptionBefore == Empty
   and remove DescriptionAfter == Empty instead of re-validating these boundaries.
3. Frozen Status snapshots enforce RuntimeSequence ordering/uniqueness, while duplicate
   StatusId rejection is enforced later by the reducer rather than explicitly at freeze time.
```

These are recommended additions before A2D-5 combined acceptance, ideally with focused malformed-history tests.

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
UE5.8 Editor build                         PASS
Phase6UIA2D3 Automation                    PASS 4/4
88-test affected regression                PASS 88/88
```

A2D-3 is closed at the C++/Automation level and is ready for A2D-4.

See `docs/Phase6UIA2D3Validation.md` for the dedicated validation record.
