# Phase 6UI-A2D3 Static Source Review

Date: **2026-08-22**

Status: **POST-VALIDATION HARDENING COMPLETE / UE5.8 REVALIDATION PENDING**.

A2D-3 implements the locked Status historical projection slice: `FBattleHUDStatusView.RuntimeSequence`, deterministic frozen Status ordering, `StatusChanged` WorkingPresentationSnapshot reduction, and mismatch collapse to the immutable `Envelope.FinalSnapshot`.

The original implementation previously passed focused A2D-3 Automation **4/4** and affected Phase6R **88/88**. Subsequent review hardening changed runtime and test source, so those results are retained as historical validation of the pre-hardening base only. The current hardened head must be rerun before A2D-3 is marked validated again.

## Implemented and hardened scope

```text
FBattleHUDStatusView.RuntimeSequence
Gameplay uint64 -> Presentation int64 freeze guard
FinalSnapshot Status RuntimeSequence sorting
strict frozen RuntimeSequence uniqueness/order
explicit frozen StatusId uniqueness
StatusChanged visible-playback participation
pre-playback Status reducer validation on WorkingSnapshot copy
WorkingPresentationSnapshot Status reducer
exact StatusId + RuntimeSequence matching
SourcePresentationId participant validation
AmountBefore chain validation
create / increase / reduce / remove projection
create/remove Description structural validation
reducer mismatch -> FinalSnapshot collapse before Blueprint playback
Controller bootstrap baseline repair before watermark advance
```

A2D-3 does not query live Gameplay from the Controller reducer or preflight path.

## Frozen snapshot identity and ordering

`FStatusReadView` carries Gameplay `uint64 RuntimeSequence`; Presentation freezes it as `int64` only after validating:

```text
StatusId != NAME_None
Amount > 0
RuntimeSequence > 0
RuntimeSequence <= MAX_int64
StatusId unique within the combatant frozen array
```

Statuses are sorted by `RuntimeSequence ascending`, then strict ordering verifies RuntimeSequence uniqueness.

The authoritative freeze boundary therefore establishes the same StatusId/RuntimeSequence uniqueness assumptions later required by the reducer.

## Producer source identity

Only an actually absent Source is anonymous:

```text
Source == nullptr
-> SourcePresentationId == NAME_None
```

A supplied non-null Source must remain valid and resolve through the authoritative BattleManager participant resolver. A supplied invalid/pending-kill object cannot be silently converted into `NAME_None`; it invalidates the current unpublished Presentation history while leaving committed Gameplay intact.

## WorkingSnapshot reducer and preflight

Concrete status identity is:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

The Status reducer validates current projection invariants, Source/Target participant identity, immutable payload shape, reason/amount semantics, Description structural boundaries, exact runtime identity, and AmountBefore continuity.

Before a `StatusChanged` Record is offered to Blueprint, `StartNextRecord` now performs the same reducer on a temporary copy of `WorkingPresentationSnapshot`.

```text
valid preflight
-> discard temporary result
-> offer Record to Blueprint
-> real WorkingSnapshot remains unchanged during animation
-> callback/native fallback/timeout
-> apply Record to real WorkingSnapshot

invalid preflight
-> do not call Blueprint PlayPresentationRecord
-> collapse directly to Envelope.FinalSnapshot
```

This prevents known-corrupt historical facts from producing a visible animation before recovery.

## Description boundaries

Producer and reducer both enforce:

```text
create -> DescriptionBefore == Empty
remove -> DescriptionAfter == Empty
```

Empty authored descriptions in other legitimate states remain allowed.

## Stale mutation semantics

Gameplay mutation classification now distinguishes malformed arguments from valid stale identity:

```text
structurally invalid stale instance -> Invalid
structurally valid old instance absent from Container -> NoOp
current exact instance -> normal mutation semantics
```

Structure is validated before Container membership in both exact Reduce and exact Remove.

## Controller bootstrap

Controller initialization explicitly takes Presentation display ownership when committed Presentation is active. If a frozen baseline exists, it is idempotently applied to the Controller/ViewModel before Resolution watermarks are raised.

A stale or newly rebuilt ViewModel therefore cannot remain visually behind while older Envelopes are suppressed by an already-advanced watermark.

## Focused Automation source

The same four top-level A2D-3 tests remain, so the dedicated workflow still expects exactly four:

```text
SlayTheSpireDemo.Phase6UIA2D3.Snapshot.RuntimeSequenceSorting
SlayTheSpireDemo.Phase6UIA2D3.Playback.StatusLifecycleReducer
SlayTheSpireDemo.Phase6UIA2D3.Safety.StaleRuntimeSequenceCollapses
SlayTheSpireDemo.Phase6UIA2D3.Safety.AmountMismatchCollapses
```

Coverage has been extended to include:

```text
duplicate frozen StatusId rejection
stale/rebuilt ViewModel bootstrap repair
stale RuntimeSequence rejected before Blueprint call
AmountBefore mismatch rejected before Blueprint call
fake SourcePresentationId rejected before Blueprint call
create with illegal DescriptionBefore rejected before Blueprint call
remove with illegal DescriptionAfter rejected before Blueprint call
```

A2D-1 and A2D-2 existing top-level tests were also extended for malformed stale identity and supplied-invalid Source behavior without changing their expected counts.

## CI / regression expected counts

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

No workflow count update is required because hardening was added inside existing top-level tests.

## Static re-review

The requested seven findings were reviewed after implementation. No high-confidence compile/UHT blocker was identified by source inspection, and the intended normal-path ordering remains:

```text
valid record preflight
-> Blueprint playback
-> completion
-> real WorkingSnapshot commit
-> exact FinalSnapshot reconciliation at Envelope completion
```

The authoritative validation requirement is now:

```text
UE5.8 Editor build                         PENDING RERUN
Phase6UIA2D1 Automation 3/3                PENDING RERUN
Phase6UIA2D2 Automation 4/4                PENDING RERUN
Phase6UIA2D3 Automation 4/4                PENDING RERUN
Phase6R aggregate 88/88                    PENDING RERUN
```

Do not mark the hardened current head `VALIDATED / READY FOR A2D-4` until those checks pass.

## Explicitly out of A2D-3

```text
A2D-4 typed terminal payloads
A2D-4 Victory / Defeat / ResolutionFault formal terminal reducer/playback
A2D-4 terminal ViewModel timing
A2D-5 combined acceptance
unified Blueprint visual integration
PIE smoke
```

See `docs/Phase6UIA2D3Validation.md` and `docs/Phase6UIA2DCurrentCodeReview.md` for the validation history and hardened review record.
