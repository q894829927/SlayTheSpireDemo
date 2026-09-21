# P1A-7 shared scratch lifecycle audit

Date: 2026-09-21  
Branch: `portal/full-fidelity-p1`  
Primary scratch implementation: `6df29d708d4bcc0d435a57162b0e2ee91cfcad33`  
Status: **IMPLEMENTED / USER VALIDATION REQUIRED**

## Scope

P1A-7 audits and hardens the single producer-wide `FinalScratch` used by the
accepted FullFidelity MultiVisible renderer.

The authoritative P1 plan requires:

```text
one expected producer scratch target
viewport-size changes do not leak old generations
Stop/Restart releases and recreates correctly
no legacy renderer scratch survives accidental dual-path execution
scratch replacement does not depend on unnecessary repeated runtime flushes
do not split scratch per recursion level
```

This step does not start P1B, does not change recursion depth policy, does not
change portal target resolution, and does not attempt to solve the pre-existing
physical traversal hitch.

## Audit result before P1A-7

The accepted producer had exactly one active `FinalScratch`, shared by all
endpoint/recursion submissions.

However, viewport-size replacement used:

```text
size changed
-> FlushRenderingCommands()
-> drop old FinalScratch
-> allocate new FinalScratch
```

That made normal resize replacement synchronously depend on a full render-command
flush.

Stop already had an intentional synchronous teardown flush, which remains
allowed by the P1 contract.

The production `AInteriorPlayerController` already returns from its FullFidelity
branch before calling legacy `PortalSystem->RenderViews()` or
`UInteriorPortalFullSceneViewSubsystem::Render()`, so the normal production
path does not dual-submit those renderers.

The audit also found four historical console-only persistent scratch producers:

```text
portal.StartFullViewFamilyRealtimeSpike
portal.StartFullViewFamilyTSRSpike
portal.StartExposureAuthoritySpike
portal.StartSecondaryEyeAdaptationSpike
```

Those diagnostic producers could previously be started alongside the accepted
backend and each could retain its own scratch target.

## Implemented shared-scratch contract

Commit `6df29d708d4bcc0d435a57162b0e2ee91cfcad33` replaces the
`EnsureFinalScratch()` resize flush with bounded queue-safe retirement.

Stable state:

```text
active scratch = 1
retiring scratch = 0
owned scratch = 1
```

On viewport-size change:

```text
allocate replacement
-> old scratch enters FRetiringScratchTarget
-> begin RHI-thread-depth FRenderCommandFence
-> new scratch becomes active immediately
-> poll old scratch on later ticks
-> after fence completion:
   ReleaseResource()
   RemoveFromRoot()
   remove old generation
```

At most one retired scratch generation is allowed:

```text
MaxRetiringScratchTargets = 1
```

Therefore normal ownership is bounded to:

```text
steady state: 1
resize transition: <= 2
```

If another resize arrives before the prior generation finishes, the producer
does not allocate an unbounded third full-size scratch and does not call
`FlushRenderingCommands()`. It temporarily skips that portal submission and
increments `replacementDeferredCount`; a later tick retries after retirement
progresses.

`UpdateResourceImmediate(true)` is still used to materialize a newly created
render target. P1A-7 only removes the prior explicit full
`FlushRenderingCommands()` dependency from normal scratch replacement; it does
not claim that GPU allocation/resource creation itself has zero cost.

## Stop / Restart

Stop remains the synchronous teardown boundary:

```text
FlushRenderingCommands()
-> drain any retiring scratch generations
-> ReleaseResource() active FinalScratch
-> RemoveFromRoot()
-> active generation = 0
-> producer reset by backend lifecycle
```

Restart creates a new producer and lazily creates one new shared scratch when
rendering first needs it.

## Legacy dual-path hardening

The accepted backend now exposes its running ownership state through
`InteriorPortalFullFidelityBackend::IsRunning()`.

Historical persistent scratch producers now:

```text
FullFidelity already running
-> legacy Start refuses

legacy producer already running
-> FullFidelity begins owning rendering
-> legacy producer detects ownership on next world tick
-> legacy Stop
-> existing teardown Flush
-> explicit scratch ReleaseResource()
-> RemoveFromRoot()
```

The legacy scratch teardown functions were updated to explicitly
`ReleaseResource()` after their existing synchronous Stop/resize flush rather
than relying on a later UObject GC pass.

One-shot historical validation functions that create local
`TStrongObjectPtr<UTextureRenderTarget2D>` scratch objects are not persistent
producer ownership and are not part of this steady-state scratch contract.

## Diagnostics

The report schema is now:

```text
PortalFullFidelityPingPongViewport.Prototype.v6
```

`sharedScratch` reports:

```text
allocated
generation
width
height
renderTargetFormat
estimatedBytes
activeCount
retiringCount
ownedCount
maxRetiringCount
replacementDeferredCount
retiring[]
```

Each retiring scratch record reports:

```text
generation
fenceComplete
allocated
width
height
renderTargetFormat
estimatedBytes
```

Console dump also reports:

```text
Scratch=<width>x<height>
ScratchGeneration=<id>
RetiringScratch=<n>
OwnedScratch=<n>
ScratchResizeDeferred=<n>
```

The global `totals.explicitTargetEstimatedBytes` includes both the active and
temporarily retiring scratch generations.

Logical ownership and explicit resource release still do not prove exact
immediate process-wide D3D12 allocator residency return.

## Validation not performed by the implementation environment

The GitHub-only implementation environment cannot run the user's UE 5.8 build,
Automation, D3D12 PIE, interactive viewport resize, or legacy-console coexistence
test.

Do not mark P1A-7 COMPLETE / VALIDATED until the following gates are recorded.

## USER ACTION REQUIRED

### 1. Development Editor build

Run the same UE 5.8 Development Editor build used for P1A-5/P1A-6.

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

### 3. Stable shared-scratch ownership

Use a low-pressure configuration:

```text
portal.FullFidelityPingPong 0
RequestedDepth = 1
```

Warm the portal and run:

```text
portal.DumpMultiVisibleTSRSpike
```

Expected stable v6 report:

```text
sharedScratch.allocated = true
sharedScratch.activeCount = 1
sharedScratch.retiringCount = 0
sharedScratch.ownedCount = 1
sharedScratch.maxRetiringCount = 1
```

There must never be one scratch per recursion level or endpoint.

### 4. Viewport-size replacement

Prefer **New Editor Window (PIE)** so the play window can be resized directly.

With RequestedDepth=1:

1. warm at the initial window size and dump;
2. resize the PIE window once to a clearly different aspect/size;
3. dump immediately if practical;
4. wait about one second and dump again;
5. resize several times quickly, then allow it to settle and dump again.

Expected:

```text
generation advances when the scratch size changes
activeCount = 1
retiringCount <= 1
ownedCount <= 2
settled retiringCount = 0
settled ownedCount = 1
```

A rapid resize may increment `replacementDeferredCount`. That is expected
backpressure, not a leak.

The accepted scratch implementation itself no longer calls
`FlushRenderingCommands()` during normal size replacement. Other target types
still have their own size-change behavior, so this Gate does not claim the whole
renderer resize frame is synchronization-free.

Visual expectation: no crash/assert and no persistent blank or stale portal
after resizing settles. A transient skipped portal submission under extreme
resize churn is permitted by the bounded backpressure policy.

### 5. Stop / Restart

After stable rendering:

```text
portal.StopMultiVisibleTSRSpike
portal.DumpMultiVisibleTSRSpike
```

The STOPPED v6 report must show:

```text
sharedScratch.allocated = false
sharedScratch.activeCount = 0
sharedScratch.retiringCount = 0
sharedScratch.ownedCount = 0
```

Then:

```text
portal.StartMultiVisibleTSRSpike
```

Warm again and dump. Expect exactly one active shared scratch and no old retiring
generation.

### 6. Legacy dual-path guard

While MultiVisible FullFidelity is running, issue:

```text
portal.StartFullViewFamilyTSRSpike
```

Expected log:

```text
PortalTSRSpike: refusing to start while accepted FullFidelity backend owns rendering.
```

Then test the reverse takeover:

```text
portal.StopMultiVisibleTSRSpike
portal.StartFullViewFamilyTSRSpike
portal.StartMultiVisibleTSRSpike
```

On the next world tick, expect the historical producer to log that it is stopping
because the accepted FullFidelity backend acquired rendering ownership, followed
by its normal STOP log.

Finally dump MultiVisible again. It must remain functional with exactly one
steady-state shared scratch.

Testing the TSR historical producer is sufficient for this manual coexistence
Gate because all four persistent historical scratch producers use the same new
backend-ownership guard and the same explicit scratch-release rule.

## Acceptance

P1A-7 becomes **COMPLETE / VALIDATED** when:

- Development Editor build passes;
- focused FullFidelity Automation passes;
- steady-state shared scratch ownership is exactly one;
- viewport resize settles back to one scratch and never exceeds two owned
  generations;
- Stop reports zero active/retiring scratch ownership;
- Restart recreates exactly one active scratch;
- the legacy TSR scratch producer is rejected while FullFidelity owns rendering;
- reverse takeover makes the legacy producer Stop and release its scratch;
- no crash/assert or persistent blank/stale portal is introduced.

After P1A-7 closes, execute the **full P1A Gate** before beginning P1B.
