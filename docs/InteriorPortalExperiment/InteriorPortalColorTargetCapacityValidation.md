# P1A-5 legacy color-target capacity shrink

Date: 2026-09-21  
Branch: `portal/full-fidelity-p1`  
Implementation commit: `6404baf08f0fcfce0a3dd21ed1aa5fb94da541dd`  
Status: **IMPLEMENTED / D3D12 CORE MATRIX PASS / BUILD + AUTOMATION + RAPID-REVERSAL CONFIRMATION PENDING**

## Scope

P1A-5 changes only the legacy/fallback per-recursion-level **color RenderTarget**
capacity used when:

```text
portal.FullFidelityPingPong = 0
```

It does not implement P1A-6 depth-target policy, P1A-7 shared-scratch cleanup,
P1B screen-coverage cutoff, traversal hitch work, or Portal physics.

Ping-Pong color ownership remains the existing endpoint-shared one/two-slot
model and is not converted back into per-level resources.

## Implemented contract

Before this change, `AInteriorPortal::RenderTargets` only grew through
`EnsureTargets()`. A warmed depth-four fallback endpoint could therefore keep
four color targets after RequestedDepth dropped to one.

P1A-5 now couples an out-of-budget fallback color target to the exact
`FRetiringLayer` that last submitted into it:

```text
RequestedDepth 4 -> 1
    -> L1/L2/L3 lifetime enters RETIRING
    -> matching fallback Color RT is removed from active RenderTargets
    -> detached Color RT is rooted while retirement is pending
    -> old publisher/child consumer clear in queue order
    -> existing RHI-thread-depth retirement fence completes
    -> Color RT ReleaseResource()
    -> RemoveFromRoot()
    -> old lifetime becomes reclaimable/unallocated
```

The active `RenderTargets` array is shrunk in descending recursion-level order,
so retained lower-level indices remain stable. A later `1 -> 4` uses the
existing `EnsureTargets()` grow path to create fresh L1/L2/L3 color targets.
Because the color target retires with its layer lifetime, the existing P1A-4
old/new lifetime backpressure also bounds rapid reversal generations.

Capacity is driven only by `RequestedDepth` through `UpdateCapacity()`.
Short-term invisibility does not retire a color target. A future P1B reduction
of `EffectiveDepth` must likewise not be treated as a capacity shrink.

`Stop()` remains the synchronous teardown boundary. After the existing render
flush, it drains retiring layers/color targets and also releases any active
fallback color targets before clearing their actor-owned array.

## Diagnostics

The report schema is now:

```text
PortalFullFidelityPingPongViewport.Prototype.v4
```

Each endpoint reports:

```text
colorTargetCount
activeColorTargetCount
retiringColorTargetCount
```

`colorTargetCount` is logical owned color-target count, so during asynchronous
retirement it includes both active and retiring fallback targets.

Each `retiringLifetimes[]` record now also reports its optional
`legacyColorTarget` allocation, dimensions, format and estimated bytes.

Console dump output also prints:

```text
ColorTargets Active=<n> Retiring=<n> Owned=<n>
```

Logical release/estimated bytes do not by themselves prove that the global
D3D12 allocator has immediately returned the same number of resident bytes.

## User-provided D3D12 evidence — 2026-09-21

The user supplied actual PIE dump output with `portal.FullFidelityPingPong 0`
and report schema v4.

Observed settled depth-four state:

```text
RequestedDepth = 4
Endpoint 0 ColorTargets Active=4 Retiring=0 Owned=4
Endpoint 1 ColorTargets Active=4 Retiring=0 Owned=4
```

Observed settled shrink:

```text
RequestedDepth = 1
Endpoint 0 ColorTargets Active=1 Retiring=0 Owned=1
Endpoint 1 ColorTargets Active=1 Retiring=0 Owned=1
L1-L3 = UNALLOCATED
```

Observed settled regrowth:

```text
RequestedDepth = 4
Endpoint 0 ColorTargets Active=4 Retiring=0 Owned=4
Endpoint 1 ColorTargets Active=4 Retiring=0 Owned=4
```

The retained L0 lifetime identities remained stable across the shrink/regrowth
(`E0L0 Lifetime=1`, `E1L0 Lifetime=5`) while rebuilt L1-L3 received fresh
identities (`E0: 9/10/11`, `E1: 12/13/14`). This is consistent with the
P1A-4/P1A-5 capacity model.

Observed offscreen invariance at RequestedDepth 4:

```text
VisibleEndpointMask = 0
Endpoint 0 ColorTargets Active=4 Retiring=0 Owned=4
Endpoint 1 ColorTargets Active=4 Retiring=0 Owned=4
```

Therefore visibility dropping to zero did not shrink configured color capacity.

Observed Stop teardown report:

```text
status = STOPPED
totals.viewStates = 0
totals.colorTargets = 0
totals.depthTargets = 0
totals.explicitTargetEstimatedBytes = 0
endpoint activeColorTargetCount = 0
endpoint retiringColorTargetCount = 0
```

This establishes the core actual-D3D12 P1A-5 capacity behavior for settled
`4 -> 1 -> 4`, offscreen invariance, and Stop teardown.

The supplied dump sequence does **not** prove a deliberately rapid
`4 -> 1 -> 4` reversal before the old retirement fence completes, because all
captured states are already settled with `Retiring=0`. It also does not contain
the formal UE 5.8 Development Editor build or focused FullFidelity Automation
result. Those remain pending acceptance evidence.

## Validation not performed by this change

The GitHub implementation environment itself did not run local UE 5.8 build,
Automation, D3D12 PIE, or GPU-memory capture. Actual D3D12 PIE evidence above is
user-provided. Do not mark P1A-5 COMPLETE / VALIDATED until the remaining gates
below are satisfied.

## USER ACTION REQUIRED

### 1. Regenerate and build

Run from PowerShell:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\ThirdParty\DotNet\10.0\win-x64\dotnet.exe" "E:\Unreal engine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll" -ProjectFiles -project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" -game -engine -2022
```

Then:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Build\BatchFiles\Build.bat" SlayTheSpireDemoEditor Win64 Development -Project="E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" -WaitMutex -FromMsBuild -2022 -architecture=x64
```

Expected: both commands finish successfully.

### 2. Focused Automation regression

Run the existing focused FullFidelity tests, at minimum:

```text
SlayTheSpireDemo.Interior.Portals.FullFidelity
```

The important regression is that the P1A-2/P1A-4 lifetime tests remain PASS.
This change does not claim a new synthetic Automation test can prove D3D12
RenderTarget retirement; the actual resource transition is checked in PIE below.

### 3. Fallback D3D12 capacity matrix

Use:

```text
/Game/House/L_Interior_LivingKitchen
portal.FullFidelityPingPong 0
```

The Ping-Pong setting is sampled when the FullFidelity producer starts. Set it
before a fresh PIE, or stop/restart the producer after changing it.

Warm a real recursive view at RequestedDepth 4, then run:

```text
portal.DumpMultiVisibleTSRSpike
```

For each endpoint that has actually been warmed to four layers, expect settled:

```text
RequestedDepth = 4
activeColorTargetCount = 4
retiringColorTargetCount = 0
colorTargetCount = 4
```

Change RequestedDepth to 1. After retirement settles, dump again:

```text
RequestedDepth = 1
activeColorTargetCount = 1
retiringColorTargetCount = 0
colorTargetCount = 1
```

If a dump catches the asynchronous transition before its fence finishes, this
temporary state is valid:

```text
activeColorTargetCount = 1
retiringColorTargetCount = 1..3
colorTargetCount > 1
```

The retiring records must subsequently disappear without
`FlushRenderingCommands()` in the normal capacity-change path.

### 4. Regrowth and rapid reversal

Exercise:

```text
4 -> 1
1 -> 4
rapid 4 -> 1 -> 4
```

After settled `1 -> 4`, a warmed endpoint should again report four active
color targets with zero retiring color targets.

During rapid reversal, old and new generations may coexist temporarily, but:

- no stale portal publication may replace the new output;
- no black/blank aperture should persist;
- no crash/assert should occur;
- retirement must settle back to zero;
- active fallback Color RT count must match RequestedDepth once the endpoint is
  actually warmed to that depth.

### 5. Offscreen invariance

Keep RequestedDepth at 4 after warming an endpoint, turn fully away so the
portal becomes temporarily invisible, and dump again.

Expected:

```text
RequestedDepth = 4
activeColorTargetCount = 4
```

Visibility alone must not shrink fallback color capacity.

### 6. Stop / Restart

Run:

```text
portal.StopMultiVisibleTSRSpike
```

The stopped report must show no FullFidelity-owned active or retiring color
targets. Restart and warm the fallback path again; fresh targets must be created
without stale publications.

## Acceptance rule

P1A-5 may be marked **COMPLETE / VALIDATED** only after:

- Development Editor build PASS;
- focused FullFidelity Automation regression PASS;
- actual D3D12 fallback `4 -> 1 -> 4` target counts behave as above;
- offscreen visibility does not shrink capacity;
- rapid reversal has no stale/blank/crash regression;
- Stop/Restart leaves no old FullFidelity color-target ownership.

P1A-5 does not require solving the pre-existing traversal hitch or the sustained
Depth=4 scene-rendering cost. Those remain separate issues.
