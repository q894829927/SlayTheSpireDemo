# P1A-6 depth-target capacity reclaim

Date: 2026-09-21  
Branch: `portal/full-fidelity-p1`  
Implementation commit: `583181eb35786548ccc3604d0d72278dc0e170db`  
Status: **IMPLEMENTED / D3D12 CORE MATRIX PASS / AUTOMATION + RAPID-REVERSAL CONFIRMATION PENDING**

## Scope

P1A-6 closes the per-recursion-level secondary depth-target capacity contract.

The authoritative plan requires:

```text
depth target creation remains lazy on submission
in-budget owned level may retain its depth target across short visibility changes
out-of-budget old lifetime retires and releases its depth target only when safe
no P1 depth-resolution change
```

This step does not change P1A-5 color-target policy, Ping-Pong projected target
resolution, shared scratch ownership, P1B recursion selection, traversal, or
Portal physics.

## Code audit result

The P1A-4 lifetime implementation had already established most of the correct
ownership path:

```text
FLayerState owns SecondaryDepthTarget
RequestedDepth reduction
    -> whole old FLayerState moves into FRetiringLayer
    -> RHI-thread-depth Fence is started
PollRetirements
    -> waits for Fence
    -> ReleaseDepthTarget(old layer)
```

Therefore P1A-6 does not introduce a second retirement mechanism.

The remaining gap was that `ReleaseDepthTarget()` only removed the UObject from
the root set and dropped the pointer. That made logical ownership disappear but
left actual render-resource destruction dependent on a later UObject GC pass.

## Implemented contract

Commit `583181eb35786548ccc3604d0d72278dc0e170db` changes the safe release path to:

```text
retiring lifetime fence complete
    -> SecondaryDepthTarget->ReleaseResource()
    -> RemoveFromRoot()
    -> clear pointer
    -> clear cached target size
    -> lifetime becomes UNALLOCATED
```

The normal RequestedDepth capacity-change path still does not call
`FlushRenderingCommands()`.

`EnsureDepthTarget()` remains lazy. A secondary depth target is created only
when that level reaches actual submission and needs one.

Short visibility loss does not call `UpdateCapacity()` with a lower depth, so an
in-budget depth target remains owned across temporary offscreen periods.

P1A-6 does not change depth-target dimensions or pixel format.

The endpoint-shared Ping-Pong depth targets are not converted into per-level
targets by this step. Their shared scratch/capacity behavior remains outside the
per-layer secondary-depth contract.

## Diagnostics

Report schema advances to:

```text
PortalFullFidelityPingPongViewport.Prototype.v5
```

Each endpoint now exposes:

```text
depthTargetCount
activeDepthTargetCount
retiringDepthTargetCount
```

Console dump now includes:

```text
DepthTargets Active=<n> Retiring=<n> Owned=<n>
```

Each retiring lifetime reports its old secondary depth target:

```text
secondaryDepthTarget:
    allocated
    width
    height
    renderTargetFormat
    estimatedBytes
```

This allows a transition to be observed as:

```text
Depth 4 stable:
Active=4 Retiring=0 Owned=4

4 -> 1 before fence settles:
Active=1 Retiring=1..3 Owned>1

Depth 1 settled:
Active=1 Retiring=0 Owned=1
```

Logical/report ownership and explicit `ReleaseResource()` still do not prove an
exact immediate decrease in process-wide D3D12 allocator residency. Global RHI
memory remains supporting evidence rather than an equality assertion.

## User-provided D3D12 evidence — 2026-09-21

The user supplied actual fallback PIE output with report schema v5 and
`portal.FullFidelityPingPong 0`.

Observed warmed RequestedDepth=2:

```text
Endpoint 0 DepthTargets Active=2 Retiring=0 Owned=2
Endpoint 1 DepthTargets Active=2 Retiring=0 Owned=2
```

Both L0/L1 layers were ACTIVE and had real 1606x900 secondary depth targets.

Observed settled shrink to RequestedDepth=1:

```text
Endpoint 0 DepthTargets Active=1 Retiring=0 Owned=1
Endpoint 1 DepthTargets Active=1 Retiring=0 Owned=1
L1 = UNALLOCATED
```

Observed settled regrowth back to RequestedDepth=2:

```text
Endpoint 0 DepthTargets Active=2 Retiring=0 Owned=2
Endpoint 1 DepthTargets Active=2 Retiring=0 Owned=2
```

The retained L0 lifetime identities stayed stable across the cycle
(`E0L0 Lifetime=1`, `E1L0 Lifetime=2`). Rebuilt L1 received fresh identities
(`E0L1 3 -> 5`, `E1L1 4 -> 6`). This matches the lifetime/reclaim model and
demonstrates that the old L1 depth targets did not remain as active capacity.

The final STOPPED v5 report shows:

```text
totals.depthTargets = 0
endpoint activeDepthTargetCount = 0
endpoint retiringDepthTargetCount = 0
```

Therefore the actual D3D12 fallback depth-target settled `2 -> 1 -> 2` matrix
and Stop-zero-ownership gates pass.

The lower `2 -> 1 -> 2` matrix is accepted in place of forcing full fallback
Depth=4 for this resource-lifecycle gate because the same per-level retirement
path is exercised and the user observed approximately 1 GB of additional VRAM
pressure at fallback Depth=4. This avoids turning a known high-VRAM validation
case into an unnecessary OOM risk.

The earlier P1A-5 offscreen capture already showed that, with unchanged
RequestedDepth, per-level depth targets remained allocated while
VisibleDepth/EffectiveDepth dropped to zero. P1A-6 does not modify the
visibility/capacity trigger path, so that passing offscreen-retention evidence
remains applicable under the repository validation-reuse policy.

The supplied sequence is settled evidence; it does not prove a deliberately
rapid `2 -> 1 -> 2` reversal while the old depth target is still retiring.
Focused Automation after the P1A-6 code change is also not yet recorded here.

## Validation not performed by the implementation environment

The GitHub-only implementation environment cannot execute the user's UE 5.8
editor, Development Editor build, D3D12 PIE, or GPU-memory capture.

Actual D3D12 core-matrix and Stop evidence above are user-provided. P1A-6 remains
open only for the remaining affected regression gates.

## USER ACTION REQUIRED

### 1. Development Editor build

Run the standard project generation and editor build already used for P1A-5.

Expected:

```text
SlayTheSpireDemoEditor Win64 Development = PASS
```

### 2. Focused FullFidelity Automation

Run:

```powershell
& "E:\Unreal engine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "E:\UE_DEMO\SlayTheSpireDemo\SlayTheSpireDemo.uproject" `
  -unattended -nop4 -NullRHI -NoSplash -NoSound `
  "-ExecCmds=Automation RunTest SlayTheSpireDemo.Interior.Portals.FullFidelity;Quit" `
  -log
```

Expected: focused suite PASS.

### 3. Actual D3D12 secondary-depth capacity matrix

Use a fresh PIE in:

```text
/Game/House/L_Interior_LivingKitchen
portal.FullFidelityPingPong 0
```

Warm RequestedDepth 4 and run:

```text
portal.DumpMultiVisibleTSRSpike
```

For each fully warmed endpoint expect:

```text
RequestedDepth=4
DepthTargets Active=4 Retiring=0 Owned=4
```

Then set RequestedDepth to 1 and dump after retirement settles:

```text
RequestedDepth=1
DepthTargets Active=1 Retiring=0 Owned=1
L1-L3 depthTarget.allocated=false
```

A dump taken immediately during retirement may legitimately show:

```text
Active=1
Retiring=1..3
Owned>1
```

and the corresponding `retiringLifetimes[].secondaryDepthTarget.allocated`
must be true until that lifetime's fence completes.

### 4. Regrowth and rapid reversal

Exercise:

```text
4 -> 1 -> 4
rapid 4 -> 1 -> 4
```

After settling back at 4:

```text
DepthTargets Active=4 Retiring=0 Owned=4
```

No stale publication, persistent blank aperture, crash, or assert is acceptable.

### 5. Offscreen retention

At RequestedDepth 4, warm four layers, then turn both endpoints fully offscreen
without changing RequestedDepth.

Expected:

```text
VisibleDepth=0
RequestedDepth=4
DepthTargets Active=4 Retiring=0 Owned=4
```

Visibility alone must not reclaim an in-budget secondary depth target.

### 6. Stop teardown

Stop FullFidelity / end PIE and inspect the final report.

Expected:

```text
totals.depthTargets=0
activeDepthTargetCount=0
retiringDepthTargetCount=0
```

## Acceptance

P1A-6 becomes **COMPLETE / VALIDATED** when:

- Development Editor build passes;
- focused FullFidelity Automation passes;
- actual D3D12 fallback depth ownership settles `4 -> 1 -> 4` as expected;
- rapid reversal has no stale/blank/crash regression;
- offscreen visibility does not reclaim in-budget depth targets;
- Stop leaves no active or retiring depth-target ownership.

After P1A-6 closes, the next planned step is **P1A-7 Shared Scratch Lifecycle
Audit**.
