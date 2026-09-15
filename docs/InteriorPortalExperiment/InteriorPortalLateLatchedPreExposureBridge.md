# STEP 1B.14D-B — Late-Latched Main Tonemap PreExposure Bridge

Status: IMPLEMENTED / NOT YET BUILT OR RUN

The previous exposure-authority experiment captured the player-main view state during `SetupView`, but runtime data showed `GetPreExposure()` remained `1.0` there while the real main-view Tonemap callback reported about `0.00205–0.00208` in the same run.

This step adds a narrow diagnostic bridge. It captures the real main-view PreExposure at Tonemap, stores it as a late-latched scalar, then publishes the latest completed value to `portal.SecondaryPreExposure` on the following game-thread post-actor tick while keeping `portal.PreExposureRebase=1`.

It does not change the accepted TSR history policy, clipping, depth propagation, stencil composition, or aperture math.

Commands:

```text
portal.LateLatchedPreExposureDiagnostics 1
portal.StartLateLatchedPreExposureBridge
portal.DumpLateLatchedPreExposureBridge
portal.StopLateLatchedPreExposureBridge
```

Start the portal producer first, then start this bridge.

Expected telemetry:

```text
PortalLateLatch MainTonemap ... PreExposure=0.002...
PortalLateLatch Publish ... Before=1 Published=0.002... Rebase=1
```

If the portal remains stable while the bridge is running, the next action is to move this late-latched exposure value into the production TSR producer and remove the temporary bridge. If it does not, the next investigation moves to secondary view-family ownership, endpoint selection, clipping, or Lumen history.

Remaining work should not be described as a fixed one-or-two-step task. Current bounded estimate from this point is 3 gates minimum, 3–4 likely, and up to 5 if endpoint/clipping or Lumen history also needs correction:

1. `1B.14D-B` late-latched exposure bridge A/B — current step.
2. `1B.14D-C` dynamic slant / retreat / leave-and-return validation.
3. `1B.14E` production merge plus full depth/stencil/TSR regression.
4. Optional endpoint/clip-plane correction if still required.
5. Optional Lumen/additional-view history correction if still required.
