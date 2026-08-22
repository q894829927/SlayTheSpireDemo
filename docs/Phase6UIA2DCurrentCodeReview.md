# Phase 6UI-A2D Current Code Review

Date: **2026-08-22**

Status: **REVIEW FINDINGS FIXED / UE5.8 REVALIDATED / READY FOR A2D-4**.

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

The previously reported seven hardening findings have been resolved in source and covered by Automation. The hardened current head has completed its UE5.8 affected regression rerun successfully.

```text
UE5.8 Editor build                 PASS
Phase6UIA2D1                       PASS 3/3
Phase6UIA2D2                       PASS 4/4
Phase6UIA2D3                       PASS 4/4
Phase6R aggregate                  PASS 88/88
```

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

## Resolved 1 — supplied invalid Source no longer becomes anonymous

`Source == nullptr` is the only path that may freeze `NAME_None`. A supplied non-null Source must be valid and resolve through the authoritative BattleManager participant resolver.

## Resolved 2 — Controller validates SourcePresentationId

Status reducer validation accepts only `NAME_None` or a participant PresentationId already present in the WorkingSnapshot.

## Resolved 3 — description structural boundaries are double-checked

Controller mirrors the producer rules:

```text
bCreated && DescriptionBefore non-empty -> reject
bRemoved && DescriptionAfter non-empty  -> reject
```

## Resolved 4 — freeze/reducer StatusId uniqueness is symmetric

`FreezeCombatant` rejects duplicate `StatusId`; RuntimeSequence strict ordering/uniqueness remains enforced after sorting.

## Resolved 5 — Status Record is validated before Blueprint playback

For `StatusChanged`, Controller performs a reducer preflight on a copy of `WorkingPresentationSnapshot`.

```text
invalid -> no Blueprint call -> collapse FinalSnapshot
valid   -> Blueprint playback -> completion -> real WorkingSnapshot commit
```

This preserves display timing while preventing visibly playing known-corrupt history.

## Resolved 6 — structurally invalid stale instance is Invalid, not NoOp

`ReduceStatusCommit` and `RemoveStatusCommit` validate complete historical identity before checking exact Container membership.

```text
malformed stale instance -> Invalid
valid stale old instance -> NoOp
current exact instance    -> normal mutation path
```

## Resolved 7 — Controller bootstrap repairs stale ViewModel before watermarking

Controller explicitly takes Presentation display ownership, applies the latest frozen baseline, then advances Resolution watermarks. Correctness no longer depends on Presenter/ViewModel initialization order.

## Regression coverage

No new top-level prefix was added, so CI counts remain:

```text
A2D1  3
A2D2  4
A2D3  4
Phase6R total 88
```

Coverage proves invalid stale identity classification, invalid supplied Source behavior, pre-playback rejection of fake Source/description/stale amount history, duplicate frozen StatusId rejection, normal post-animation commit timing, and bootstrap repair of stale ViewModel state.

## Remaining observation — legacy ReduceStatusAction overload

The compatibility overload:

```text
Initialize(UStatusContainer*, UStatusInstance*, int32)
```

still intentionally lacks authoritative Battle context. It remains a low-severity API hygiene item rather than a defect in the validated A2D1-A2D3 path. A2D-5 should remove it after migration or explicitly constrain it to no-history/test use.

## Review closure

The seven requested findings are closed and the hardened A2D-1 through A2D-3 status path is now **VALIDATED / READY FOR A2D-4**.
