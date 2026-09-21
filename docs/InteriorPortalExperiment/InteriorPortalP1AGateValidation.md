# Full P1A Gate validation

Date: 2026-09-21  
Branch: `portal/full-fidelity-p1`  
Authority: `docs/PortalPerformanceVRAMP1Plan.md §7`  
Status: **COMPLETE / VALIDATED / SEALED**

## Purpose

This is the aggregate seal for P1A. It does not reopen already validated P1A-4,
P1A-5, P1A-6 or P1A-7 work unless the final-head change affects the contract
that supplied their evidence.

Per `docs/ValidationExecutionPolicy.md`, passing evidence is reused unless
affected code/test/fixture changes invalidate it.

P1B is not authorized until this Gate closes.

## Final P1A code delta

The aggregate audit found one production gap in §7.3.

Submission is queued deepest -> shallowest. Before this Gate, a parent attached
`RecursiveCompositionExtensions[Level + 1]` based only on visible depth and the
existence of the extension. If that child failed `SubmitLayer()` this frame,
the extension could still contain a previous-frame publication.

The production rule is now:

```text
child SubmitLayer succeeds this frame
-> child bit enters CurrentFrameSubmittedLayerMask
-> shallower parent may consume that child

child SubmitLayer fails this frame
-> truncate EffectiveDepth to the failed child level
-> invalidate that child's prior publication
-> parent continues as a recursion terminator
-> parent must not attach/consume that child unless the child bit is present
   in the current-frame SubmittedLayerMask
```

Implemented in:

```text
4db81169f5ea28fb51508be99a629085f2ea2226
portal: define current-frame recursion failure policy

1bd1ad1b943fd4a317c6a9acc786484b4bf8606e
portal: terminate recursion on current-frame child failure

295721ba72431eec6b49b43796665c18d2e61c0b
portal: expose current-frame child consumability

b911fa95b5e501104f2f42edc0f276853906f833
test(portal): cover P1A gate failure and capacity contracts
```

Report schema advances to:

```text
PortalFullFidelityPingPongViewport.Prototype.v7
```

Each layer now reports:

```text
attemptedThisFrame
submittedThisFrame
published
consumableByParentThisFrame
submissionFailureReason
```

`published=true` by itself is no longer interpreted as a current-frame-valid
child. A queued/old publication may remain visible in diagnostics until ordered
retirement; `consumableByParentThisFrame` is the explicit current-frame contract.

## §7.1 Build

Final-head build is required because the aggregate Gate added C++ production and
Automation code after the last P1A-7 build.

User-confirmed final-head result:

```text
SlayTheSpireDemoEditor Win64 Development = PASS
```

## §7.2 Lifecycle automation

The final focused prefix now contains 10 deterministic tests:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity.AnalyticApertureGeometry
SlayTheSpireDemo.Interior.Portals.FullFidelity.RecursiveRenderRequest
SlayTheSpireDemo.Interior.Portals.FullFidelity.OwnershipPolicy
SlayTheSpireDemo.Interior.Portals.FullFidelity.ProjectedBounds.CropMath
SlayTheSpireDemo.Interior.Portals.FullFidelity.ProjectedBounds.PingPongPolicy
SlayTheSpireDemo.Interior.Portals.FullFidelity.ProjectedBounds.ReceivingViewCoordinates
SlayTheSpireDemo.Interior.Portals.FullFidelity.P1A2.LifetimeModel
SlayTheSpireDemo.Interior.Portals.FullFidelity.P1A4.CapacityReversal
SlayTheSpireDemo.Interior.Portals.FullFidelity.P1AGate.SubmissionFailureContract
SlayTheSpireDemo.Interior.Portals.FullFidelity.P1AGate.CapacityInvariants
```

Existing lifetime/capacity tests already cover:

```text
stable/rebuilt identity behavior
retiring lifetime receives no new submission
old extraction cannot publish into a rebuilt lifetime
old retirement cannot clear a rebuilt lifetime
rapid reversal backpressure
retained L0 identity
fresh rebuilt identities
```

The new aggregate tests additionally cover:

```text
failed child truncates EffectiveDepth
parent cannot consume failed/stale child
healthy child chain remains consumable
L0 failure produces zero effective depth
lower EffectiveDepth does not itself mutate configured capacity
short offscreen/effective-depth selection preserves identities
RequestedDepth reduction is the capacity-destruction signal
Stop clears old publication identity
Restart receives fresh identity
```

User-confirmed final-head focused suite result:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity = 10/10 PASS
```

## §7.3 Submission-failure contract

Implementation is complete.

For a four-level chain:

```text
L3 success
L2 failure
-> EffectiveDepth becomes 2
-> L1 cannot consume L2
-> L1 may still submit
-> L0 may consume L1 if L1 submitted this frame
```

For:

```text
L1 failure
-> EffectiveDepth becomes 1
-> L0 renders without child recursion

L0 failure
-> EffectiveDepth becomes 0
-> no valid endpoint result for this frame
```

The failed layer also goes through `HideLayer()` when it owned a previous
visible result, which advances publication generation and schedules ordered
retirement of the prior request.

This contract is deterministic and is validated by Automation; no artificial
GPU/resource failure is required in PIE.

## §7.4 PIE visual matrix — reusable evidence

The existing accepted visual evidence remains authoritative for unchanged
contracts:

- optimized crop matrix includes RequestedDepth 1/2/3/4, left/right oblique,
  partial offscreen and offscreen/return;
- the nested facing-pair fixture proved four layers submitted/published;
- user-confirmed continuous-motion acceptance covered approach/retreat,
  oblique views, partial offscreen, rapid away/back and foreground occlusion;
- P1A-4/P1A-5 user evidence covers stable and rapid `4 -> 1 -> 4` with no
  stale scene flash, prolonged blank aperture, recursion corruption or
  foreground-occlusion regression;
- the pre-existing physical traversal hitch remains a separate issue.

Do **not** rerun fallback Depth=4 solely for this aggregate seal. The project has
already observed roughly 1 GB additional fallback Depth=4 VRAM pressure and a
historical D3D12-debug-layer OOM. Those resource limits are already recorded and
do not add new evidence for the final-head child-consumption change.

### Final-head affected visual smoke — USER CONFIRMED PASS

The aggregate code changed the healthy parent/child extension attachment
condition, so one small current-head recursion smoke was required and has now
passed on the final-head v7 diagnostics.

```text
portal.FullFidelityPingPong 0
RequestedDepth = 2
```

Warm both visible portals and run:

```text
portal.DumpMultiVisibleTSRSpike
```

Expected on each visible endpoint:

```text
VisibleDepth=2
EffectiveDepth=2
Attempted=0x03
Submitted=0x03
SubmissionCount=2
Published=0x03

L1:
submittedThisFrame=true
consumableByParentThisFrame=true
submissionFailureReason=NONE
```

User-provided D3D12 PIE evidence:

```text
Endpoint 0:
VisibleDepth=2
EffectiveDepth=2
Attempted=0x03
Submitted=0x03
SubmissionCount=2
Published=0x03

Endpoint 1:
VisibleDepth=2
EffectiveDepth=2
Attempted=0x03
Submitted=0x03
SubmissionCount=2
Published=0x03
```

The final v7 JSON shows both L1 children as:

```text
submittedThisFrame=true
published=true
consumableByParentThisFrame=true
submissionFailureReason=NONE
```

The user also confirmed the two-level recursive display was visually normal with
no persistent black frame, stale scene, unexpected spiral, crash or assert.

Therefore the final-head healthy parent->child path remains intact after the
stale-child guard.

Depth=2 is sufficient for this final-head smoke because it exercises the exact
new parent -> child consumption edge. Existing accepted evidence retains the
deeper recursion matrix.

## §7.5 Capacity invariants

The report already derives:

```text
SubmissionCount = popcount(SubmittedLayerMask)
```

The runtime and aggregate Automation enforce:

```text
EffectiveDepth <= VisibleDepth <= RequestedDepth <= MaxRecursionDepth
active/reusable submission requires Level < RequestedDepth
RETIRING old lifetimes may temporarily remain above RequestedDepth
lower EffectiveDepth alone is not a persistent-capacity destruction signal
RequestedDepth reduction prevents new submission above the new ceiling
retained L0 keeps identity
Stop clears old lifetime/publication identity
Restart receives fresh identity
```

P1A-4 through P1A-6 D3D12 evidence already proves quiescent capacity shrinks to
the configured ceiling and Stop reaches zero ownership.

## §7.6 Performance-transition invariant

Existing focused Unreal Insights evidence remains valid because the aggregate
change adds no normal capacity-change flush and no new allocation path.

Recorded P1A-4 transition evidence attributes the actual Depth 1 -> 4 CPU
lifetime path to microsecond/sub-microsecond work rather than the approximately
one-second wall interval:

```text
six ViewState allocations               21 us total
six recursive-extension creations       25.9 us total
Portal_SubmitVisibleEndpoints            591.4 us
Portal_UpdateCapacity                    sub-microsecond
Portal_PollRetirements                   sub-microsecond
```

P1A-7 additionally removed the explicit `FlushRenderingCommands()` from normal
FinalScratch resize replacement.

The new child-failure policy is an integer mask/effective-depth decision and
only calls the already-existing publication invalidation path when a submission
actually fails. No new normal `4 -> 1 -> 4` flush was introduced.

No new Unreal Insights capture is required unless final-head validation reveals
a regression.

## Final-head validation — COMPLETE

The user confirmed the final-head Development Editor build PASS and the complete
FullFidelity prefix PASS 10/10. The required command was:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -unattended -nop4 -NullRHI -NoSplash -NoSound `
  "-ExecCmds=Automation RunTest SlayTheSpireDemo.Interior.Portals.FullFidelity;Quit" `
  -log
```

Recorded result:

```text
10/10 PASS
```

The Depth=2 final-head recursion smoke also passed on schema v7 with both L1
children submitted and consumable by their parents in the current frame, and
the user confirmed normal recursive visuals.

## Seal condition

Final-head P1A Gate results:

- Development Editor build: **PASS, user-confirmed**;
- FullFidelity focused Automation: **10/10 PASS, user-confirmed**;
- Depth=2 healthy recursion smoke: **PASS**, with current-frame child
  consumption confirmed by v7 diagnostics and normal visual output.

All other §7 evidence is reused from already accepted P1A/crop validation.

**FULL P1A = COMPLETE / VALIDATED / SEALED.**

P1B may now begin as the next planned stage.
