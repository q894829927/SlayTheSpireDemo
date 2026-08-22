# Phase 6UI-A2D3 Validation

Date: **2026-08-22**

Status: **PRE-HARDENING VALIDATED / CURRENT HEAD REVALIDATION PENDING**.

## Historical validated baseline

The original A2D-3 implementation was validated in UE5.8 with:

```text
Phase6UIA2D3 focused Automation   PASS 4/4
Phase6R affected regression       PASS 88/88
UE5.8 Editor build                PASS through Phase6R prerequisite
```

That result remains a valid historical result for the pre-hardening A2D-3 source base. It must not be presented as validation of the later hardening commits listed below until the gates are rerun.

## Post-validation hardening

A follow-up A2D review identified and fixed seven boundary issues:

```text
1. supplied-but-invalid non-null Status Source could collapse to NAME_None
2. Controller did not validate Status SourcePresentationId
3. Controller did not revalidate create/remove Description boundaries
4. frozen Status baseline did not explicitly reject duplicate StatusId
5. malformed/stale Status record could reach Blueprint before reducer validation
6. structurally invalid stale Status instance could be classified as NoOp
7. Controller bootstrap needed an explicit stale-ViewModel repair/ownership contract
```

Runtime fix commits:

```text
9ba552e1  fix(ui-a2d): reject invalid non-null status sources
adc82508  fix(ui-a2d): validate stale status identity before no-op
6c0b2d47  fix(ui-a2d): reject duplicate status ids at freeze boundary
82b36508  fix(ui-a2d): prevalidate status history before playback
```

Test hardening commits:

```text
6114c8e8  test(ui-a2d): distinguish invalid stale status identity from no-op
9fd6701a  test(ui-a2d): cover invalid supplied status source
79dff285  test(ui-a2d): lock hardened status playback boundaries
```

## Hardened contracts

### Source identity

Only a genuine `Source == nullptr` may freeze to `SourcePresentationId == NAME_None`.
A supplied non-null Source must be valid, belong to the authoritative battle participant set, and resolve a non-empty PresentationId.

The Controller independently validates a historical `SourcePresentationId` as either `NAME_None` or one of the participant IDs already frozen in the WorkingSnapshot.

### Description boundary

The producer and reducer now both enforce:

```text
create -> DescriptionBefore == Empty
remove -> DescriptionAfter  == Empty
```

Empty authored descriptions outside those structural boundaries remain legal.

### Frozen status uniqueness

The freeze boundary now rejects duplicate `StatusId` in addition to the existing RuntimeSequence validity and strict ordering checks.

### Playback preflight

`StatusChanged` is now preflighted against a copy of `WorkingPresentationSnapshot` before Blueprint playback.

```text
record arrives
-> copy WorkingSnapshot
-> apply/validate Status reducer on the copy
-> failure: collapse directly to Envelope.FinalSnapshot, do not call Blueprint
-> success: offer Record to Blueprint
-> callback / native fallback / timeout
-> apply the same reducer to the real WorkingSnapshot
```

The visible animation still runs against the pre-record displayed state; the real historical state advances only after playback completion.

### Stale mutation classification

`ReduceStatusCommit` and `RemoveStatusCommit` now require a complete historical identity before checking whether the exact instance is still a member of the Container.

```text
invalid Definition / StatusId / RuntimeSequence / Amount -> Invalid
valid old instance absent from Container                    -> NoOp
valid current exact instance                               -> normal commit path
```

### Controller bootstrap

Controller initialization explicitly takes Presentation display ownership when committed Presentation is active and idempotently applies the latest frozen baseline before advancing Resolution watermarks. A stale or rebuilt ViewModel is therefore repaired instead of being left behind a watermark that suppresses older Envelopes.

## Test coverage changes

The existing top-level prefixes/counts were preserved; coverage was extended inside the current tests rather than adding new top-level tests.

```text
Phase6UIA2D1 expected: 3
Phase6UIA2D2 expected: 4
Phase6UIA2D3 expected: 4
Phase6R total expected: 88
```

New coverage includes:

```text
valid stale instance -> NoOp
structurally invalid stale instance -> Invalid
invalid non-null Source -> unpublished history invalidated
fake SourcePresentationId -> pre-playback collapse
create with non-empty DescriptionBefore -> pre-playback collapse
remove with non-empty DescriptionAfter -> pre-playback collapse
duplicate frozen StatusId -> freeze failure
stale RuntimeSequence -> no Blueprint call, immediate collapse
AmountBefore mismatch -> no Blueprint call, immediate collapse
stale/rebuilt ViewModel -> Controller bootstrap reapplies baseline
```

## Current validation requirement

The post-hardening current head must be revalidated with:

```text
UE5.8 Editor build
SlayTheSpireDemo.Phase6UIA2D1  3/3
SlayTheSpireDemo.Phase6UIA2D2  4/4
SlayTheSpireDemo.Phase6UIA2D3  4/4
Phase6R aggregate             88/88
```

Do not promote the hardened current head back to **VALIDATED / READY FOR A2D-4** until these gates pass.
