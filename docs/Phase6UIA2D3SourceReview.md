# Phase 6UI-A2D3 Static Source Review

Date: **2026-08-22**

Status: **VALIDATED / READY FOR A2D-4**.

A2D-3 implements the locked Status historical projection slice: `FBattleHUDStatusView.RuntimeSequence`, deterministic frozen Status ordering, `StatusChanged` WorkingPresentationSnapshot reduction, and mismatch collapse to the immutable `Envelope.FinalSnapshot`.

The original implementation passed focused A2D-3 Automation **4/4** and affected Phase6R **88/88**. A later hardening pass changed runtime and test source; the hardened current head has now also completed the required UE5.8 regression rerun with **Phase6R 88/88 PASS**.

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

## Producer source identity

Only an actually absent Source is anonymous:

```text
Source == nullptr
-> SourcePresentationId == NAME_None
```

A supplied non-null Source must remain valid and resolve through the authoritative BattleManager participant resolver. A supplied invalid/pending-kill object invalidates the current unpublished Presentation history while leaving committed Gameplay intact.

## WorkingSnapshot reducer and preflight

Concrete status identity is:

```text
TargetPresentationId
+ StatusId
+ RuntimeSequence
```

The Status reducer validates current projection invariants, Source/Target participant identity, immutable payload shape, reason/amount semantics, Description structural boundaries, exact runtime identity, and AmountBefore continuity.

Before a `StatusChanged` Record is offered to Blueprint, `StartNextRecord` performs the same reducer on a temporary copy of `WorkingPresentationSnapshot`.

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

## Stale mutation semantics

Gameplay mutation classification is:

```text
structurally invalid stale instance -> Invalid
structurally valid old instance absent from Container -> NoOp
current exact instance -> normal mutation semantics
```

Structure is validated before Container membership in both exact Reduce and exact Remove.

## Controller bootstrap

Controller initialization explicitly takes Presentation display ownership when committed Presentation is active. If a frozen baseline exists, it is idempotently applied to the Controller/ViewModel before Resolution watermarks are raised.

## Focused Automation source

The same four top-level A2D-3 tests remain:

```text
SlayTheSpireDemo.Phase6UIA2D3.Snapshot.RuntimeSequenceSorting
SlayTheSpireDemo.Phase6UIA2D3.Playback.StatusLifecycleReducer
SlayTheSpireDemo.Phase6UIA2D3.Safety.StaleRuntimeSequenceCollapses
SlayTheSpireDemo.Phase6UIA2D3.Safety.AmountMismatchCollapses
```

Coverage includes duplicate frozen StatusId rejection, stale/rebuilt ViewModel bootstrap repair, stale RuntimeSequence and AmountBefore mismatch rejection before Blueprint, fake SourcePresentationId rejection, and create/remove Description-boundary rejection.

## CI / regression validation

Expected aggregate remains:

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

Post-hardening UE5.8 validation result:

```text
Editor build                          PASS
Phase6UIA2D1 Automation               PASS 3/3
Phase6UIA2D2 Automation               PASS 4/4
Phase6UIA2D3 Automation               PASS 4/4
Phase6R aggregate                     PASS 88/88
```

The requested seven findings are closed, no blocking defect remains in the reviewed A2D-1 through A2D-3 status path, and the hardened source is **VALIDATED / READY FOR A2D-4**.

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
