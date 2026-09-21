# P1B Recursion Screen-Coverage Cutoff validation

Date: 2026-09-21  
Branch: `portal/full-fidelity-p1`  
Authority: `docs/PortalPerformanceVRAMP1Plan.md §8`  
Status: **IMPLEMENTED / USER VALIDATION REQUIRED**

## Goal

P1B reduces deep recursive scene submissions when the next nested portal covers
very little of its current recursive parent view.

It is a workload policy only:

```text
P1B may lower EffectiveDepth
P1B does not lower RequestedDepth
P1B does not reclaim persistent in-budget capacity merely because coverage fell
```

L0 is never removed by this policy.

## Implemented policy

New runtime controls:

```text
portal.MinRecursionScreenCoverage
default = 0
candidate measurements = 0.001 / 0.0025 / 0.005

portal.RecursionCoverageHysteresisFraction
default = 0.10
```

The default threshold remains zero. No production nonzero default has been
selected before measurement and visual acceptance.

For each valid recursive request:

```text
ParentViewCoverage
= conservative padded projected portal rectangle
  / current recursive parent-view rectangle
```

The measurement uses the request's projected `ScissorRect`, expands it by the
same bounded-composition padding, clamps it to the current parent view, then
measures the area ratio.

For L1+:

```text
threshold == 0
-> include exactly as P1A

previously included level
-> stay included while coverage >= threshold * (1 - hysteresis)

previously excluded level
-> re-enter only when coverage >= threshold * (1 + hysteresis)
```

With the default 10% hysteresis this is a small Schmitt trigger, not a time
cooldown.

L0 always returns accepted regardless of threshold.

The previous coverage-selected depth is retained across a short visibility gap
for hysteresis stability, while the current diagnostic
`coverageSelectedDepth` still reports zero when no current request is visible.

## Runtime integration

Geometry still builds `VisibleDepth` before P1B selection.

Then:

```text
VisibleDepth
-> coverage policy
-> coverageSelectedDepth / initial EffectiveDepth
-> lazy ViewState/target readiness
-> actual deepest-to-shallowest SubmitLayer
-> final EffectiveDepth after any resource/submission failure
```

If an L1+ coverage test fails at level N:

```text
EffectiveDepth = N
cutoffReason = SCREEN_COVERAGE
cutoffLevel = N
levels >= N receive no SubmitLayer this frame
previous publication at N+ is invalidated through HideLayer()
persistent in-budget resource ownership is retained
```

Fallback `EnsureTargets()` now grows only to the selected effective workload
depth, but it still does not shrink targets because of a coverage decision.
Already-warmed deeper targets/ViewStates therefore remain owned until a real
capacity event such as RequestedDepth reduction or Stop.

Ping-pong likewise requests only the slots needed by current EffectiveDepth and
does not release an already-owned second slot merely because coverage drops.

## Diagnostics

Report schema is now:

```text
PortalFullFidelityPingPongViewport.Prototype.v8
```

Global policy fields:

```text
minRecursionScreenCoverage
recursionCoverageHysteresisFraction
```

Endpoint fields:

```text
visibleDepth
effectiveDepth
coverageSelectedDepth
cutoffReason
cutoffLevel
attemptedLayerMask
submittedLayerMask
submissionCount
publishedLayerMask
```

Per-level fields:

```text
parentViewCoverage
coverageTested
coverageAccepted
coverageDecisionThreshold
submittedThisFrame
consumableByParentThisFrame
submissionFailureReason
```

Console `portal.DumpMultiVisibleTSRSpike` prints the same core policy values.

## Automated coverage added

New focused test:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity.P1B.CoveragePolicy
```

It proves:

```text
parent-view area ratio math
conservative padding
threshold=0 disables P1B
L0 always survives
previously included level uses lower hysteresis boundary
previously excluded level uses upper hysteresis boundary
same coverage inside hysteresis band does not toggle state
first rejected child terminates the continuous recursion chain
```

Existing P1A aggregate tests continue to prove that lower EffectiveDepth alone
does not mutate persistent lifetime capacity.

## Validation not performed by the implementation environment

The repository-editing environment has not run the user's UE 5.8 build,
Automation, D3D12 PIE, or GPU timing on this P1B code.

Do not mark P1B complete until the following gates are recorded.

## USER ACTION REQUIRED — first functional gate

### 1. Development Editor build

Run the standard UE 5.8 Development Editor build.

Expected:

```text
SlayTheSpireDemoEditor Win64 Development = PASS
```

### 2. Focused Automation

Run:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -unattended -nop4 -NullRHI -NoSplash -NoSound `
  "-ExecCmds=Automation RunTest SlayTheSpireDemo.Interior.Portals.FullFidelity;Quit" `
  -log
```

The prefix now contains **11 tests**. Expected: 11/11 PASS.

### 3. Threshold-zero compatibility

Use the already-safe two-level scene first:

```text
portal.FullFidelityPingPong 0
RequestedDepth = 2
portal.MinRecursionScreenCoverage 0
```

Warm both visible portals and dump.

Expected:

```text
MinCoverage=0
VisibleDepth=2
EffectiveDepth=2
Attempted=0x03
Submitted=0x03
SubmissionCount=2
Cutoff=NONE
```

L1 should report:

```text
CoverageTested=1
CoverageAccepted=1
CoverageThreshold=0
```

and the v8 JSON must show
`consumableByParentThisFrame=true` for the healthy submitted L1.

This proves threshold zero reproduces the P1A workload policy.

### 4. Forced cutoff without capacity churn

Keep the exact same PIE session and camera after warming Depth=2.

Read the L1 `ParentCoverage` from the threshold-zero dump. Set
`portal.MinRecursionScreenCoverage` to a value clearly above that L1 coverage
(use a deliberately high diagnostic value for this mechanism test; this is not
a production candidate).

Expected after cutoff settles:

```text
VisibleDepth=2
coverageSelectedDepth=1
EffectiveDepth=1
Attempted=0x01
Submitted=0x01
SubmissionCount=1
Cutoff=SCREEN_COVERAGE
CutoffLevel=1
```

L0 remains submitted.

The tested L1 must show approximately:

```text
coverageTested=true
coverageAccepted=false
submittedThisFrame=false
consumableByParentThisFrame=false
```

Because the session was warmed before the cutoff, persistent ownership should
not collapse just because EffectiveDepth became one. In fallback mode, the
already-warmed endpoint should retain its in-budget L0/L1 ViewState/color/depth
ownership while RequestedDepth remains 2.

An older L1 publication may be observable for a very short ordered-retirement
window, but it must not be consumable by L0 in the cutoff frame and should clear
after retirement settles.

Set threshold back to zero. The same in-budget L1 lifetime/resource ownership
should be reusable without a capacity destroy/recreate cycle; visual recursion
must recover normally.

### 5. Hysteresis / visual stability

After the forced mechanism test passes, use a nonzero candidate threshold near
the observed L1 or deeper coverage and move/rotate slowly across the boundary.

Verify:

```text
no rapid threshold flicker
no stale nested child
no persistent black aperture
no unexpected spiral flash
oblique motion remains stable
look-away / look-back remains stable
```

The deterministic test establishes the +/-10% decision band; this PIE check is
for visible behavior.

## Candidate measurement gate — pending after functional acceptance

After the functional gate passes, compare fixed-camera scenarios using:

```text
0
0.001
0.0025
0.005
```

Record for each fixed camera:

```text
VisibleDepth
EffectiveDepth
SubmissionCount per endpoint
parent-view coverage at tested levels
GPU frame time / relevant portal rendering GPU time
visual observation
```

Use an optimized/VRAM-safe configuration for deep-recursion measurement rather
than forcing the known high-pressure fallback Depth=4 case solely for P1B.

Required scene categories:

```text
large nested portal
tiny / distant nested portal
bright or high-contrast tiny nested portal
oblique portal
look-away / look-back
```

No nonzero production default is selected until these measurements and visual
checks support one.

## Acceptance

P1B becomes COMPLETE / VALIDATED only when:

- final-head build and 11-test FullFidelity Automation pass;
- threshold zero reproduces P1A submission behavior;
- L0 survives every coverage cutoff;
- a deliberately small/distant child reduces EffectiveDepth;
- cutoff invalidates current-frame child consumption without stale visuals;
- threshold crossings remain visually stable;
- persistent ownership does not churn on EffectiveDepth-only changes;
- fixed-camera submission/GPU measurements are recorded for candidate values;
- a nonzero production default is either explicitly accepted from evidence or
  the default remains zero with the measurement conclusion recorded.
