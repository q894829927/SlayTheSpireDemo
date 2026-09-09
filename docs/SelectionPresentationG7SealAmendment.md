# Selection Presentation G7 Seal Amendment

Date: **2026-09-10**

Status: **AUTHORITATIVE CURRENT-STATUS AMENDMENT**

This amendment changes only current stage/status/resume wording after the completed G6/G7 execution. It does **not** replace the durable architecture contracts in the documents listed below.

## Superseded current-status wording

The following documents contain status banners or resume instructions written before G6/G7 completed:

```text
docs/CardSelectionPresentationConstraints.md
docs/SelectionPresentationGroupDesign.md
docs/SelectionPresentationGroupImplementationPlan.md
docs/DevelopmentPhases.md
```

Any wording in those documents equivalent to:

```text
G6 NEXT ACTIVE
G6 NOT IMPLEMENTED
G7 AFTER G6
resume from G6
remaining primary objective is G6/G7
```

is superseded by this amendment and the dedicated G6/G7 execution records.

Current status is:

```text
G0 — COMPLETE / VALIDATED / SEALED
G1 — COMPLETE / VALIDATED / SEALED
G2 — COMPLETE / VALIDATED / SEALED
G3 — COMPLETE / VALIDATED / SEALED
G4 — generic SingleRecord transition foundation retained;
     isolated pre-G5 compatibility-position PIE remains HISTORICAL FAIL
G5 — COMPLETE / VALIDATED / SEALED
G6 — COMPLETE / VALIDATED / SEALED
G7 — COMPLETE / VALIDATED / SEALED
G8 — SEPARATE DEFERRED INITIATIVE / NOT IMPLEMENTED / NOT AUTHORIZED
```

There is no automatically active Selection Presentation implementation stage after G7.

## G6 authority

`docs/SelectionPresentationG6Execution.md` is the execution/seal authority for:

```text
transactional N-child Group preflight
atomic SelectionArea -> Transition cohort transfer
Transition -> ConsumedPendingReducer
parallel visual presentation with chronological reducer order preserved
G5 sequential fallback
Native simultaneous/no-flash acceptance
```

## G7 authority

`docs/SelectionPresentationG7Execution.md` is the execution/seal authority for the post-G6 cleanup audit and validation.

G7 proved that the high-risk pre-G5 compatibility targets named by the original implementation plan had already been removed from production Source during G4+G5. The only proven dead source-level residue removed in G7 was the unused Selection-specific test alias; stale staging comments were normalized. No runtime `.cpp` behavior was changed.

## Durable contracts retained

The architectural content of the amended documents remains authoritative, including:

```text
Gameplay remains authoritative
Presentation ownership is not a Gameplay zone
Hand -> SelectionArea -> Transition -> ConsumedPendingReducer -> Done
one visible owner per RuntimeId
exact lifecycle scoping and completion-watermark recovery
G5 SingleRecord fallback
G6 transactional Group acceptance / decline
future co-presented member visual suppression without early reducer application
exact-token timeout/cancel/replacement recovery
no Effect/CardId-specific Selection destination animation
```

Historical stage sections may retain future-tense language describing what G0-G7 were required to establish. Treat that language as stage specification/history, not current implementation status.

## G8 boundary

The existing G8 design material remains a **future design candidate only**.

Do not infer authorization from its presence in the design/implementation documents. G8 requires a separate explicit user authorization and dedicated activation/acceptance scope before implementation.

Until then, do not enable:

```text
cross-Resolution early input
Presentation pipelining
detached overlapping visual-job lifetime
cosmetic NonBlocking interaction semantics
new current-revision input readiness rules
```

## Current resume rule

For future work, first identify the newly authorized goal. Do not resume Selection Presentation from G6 or G7, and do not automatically start G8.