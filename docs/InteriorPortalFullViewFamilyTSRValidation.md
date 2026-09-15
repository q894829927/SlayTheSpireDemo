# Interior Portal — STEP 1B.10B TSR / Screen-Percentage Validation

Date: **2026-09-15**

State:

```text
STEP 1B.5 FULL TRANSFORMED VIEW-FAMILY = PASS
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = PASS
STEP 1B.9 PER-FRAME FULL SECONDARY PRODUCER = PASS
STEP 1B.10A PERSISTENT SECONDARY TAA HISTORY = PASS
STEP 1B.10B TSR / TEMPORAL JITTER / SCREEN PERCENTAGE = IMPLEMENTED / NOT YET BUILT OR RUN
```

Implementation commit:

```text
9e9069e911700b5aa2cd45960e41b77b314b9048
portal: add TSR post-temporal full-view spike
```

## Important pipeline correction

STEP 1B.10A proved **history ownership and sparse camera-cut policy**. It did not
prove that the texture copied into the Portal RenderTarget already contained the
result of temporal reconstruction.

The reason is pipeline order. The existing extraction hook is `BeforeDOF`, while
UE 5.8 TSR executes after depth of field. A `BeforeDOF` SceneColor copy is
therefore necessarily pre-TSR. For the same reason, the 1B.10A PASS claim remains
valid only for persistent `FSceneViewStateReference` / camera-cut behavior; it is
not retroactively upgraded into a post-temporal PortalTexture claim.

STEP 1B.10B moves the secondary extraction point to the `Tonemap` extension pass.
That hook is after TSR but before the tonemapper, so the copied PortalTexture is
still linear HDR while also being post-temporal reconstruction.

Secondary motion blur and depth of field are disabled in this spike. The main
player view remains the owner of later presentation effects. Depth-correct DOF
through the aperture is deferred with the broader depth/stencil continuity work.

## Scope

The 1B.10B proof is intentionally independent from the already accepted
1B.9/1B.10A producer. It registers a separate command-driven producer so a bad
TSR experiment cannot regress the known-good TAA-history path.

The spike owns:

```text
one persistent secondary FSceneViewStateReference
one full-resolution RGBA16F scratch output
one endpoint RGBA16F Portal RenderTarget
one temporary post-TSR extraction extension per submitted view family
one main-view BeforeDOF portal compositor
```

The fixed primary-resolution fraction defaults to `0.67` and is latched at start.
Dynamic resolution is deliberately outside this gate.

## Renderer contract

For the secondary view:

```text
AntiAliasingMethod = AAM_TSR
bAllowTemporalJitter = true
TemporalAA show flag = true
ScreenPercentage show flag = true
FLegacyScreenPercentageDriver(global fraction = configured fraction)
EyeAdaptation = off
MotionBlur = off
DepthOfField = off
persistent ViewState = on
camera-cut policy = same accepted policy as 1B.10A
```

The output target remains full player-view resolution. The screen-percentage
driver lowers the primary render resolution; TSR is expected to reconstruct to
full output resolution before the `Tonemap` extraction callback copies linear HDR
into the portal target.

The accepted exposure-domain contract remains unchanged:

```text
PortalRGB_mainDomain = PortalRGB_secondary
                     * MainPreExposure / SecondaryPreExposure
```

## Commands

Controlled setup:

```text
RendererBackend = SceneCapture
portal.CompositionDebugMode 0
portal.CompositionDiagnostics 1
portal.FullViewFamilyTSRDiagnostics 1
portal.FullViewFamilyTSRPrimaryFraction 0.67
```

Start:

```text
portal.StartFullViewFamilyTSRSpike
```

The start command stops the older realtime spike if it is still running and
clears the one-shot compositor before taking ownership of the endpoint target.

Move and rotate the player normally for at least 10–20 seconds, including slow
lateral motion and high-contrast edges.

Dump before stopping:

```text
portal.DumpFullViewFamilyTSRSpike
```

Report:

```text
Saved/AutomationReports/PortalFullViewFamilyTSRSpike.json
```

Stop:

```text
portal.StopFullViewFamilyTSRSpike
```

## Telemetry

Periodic output is shaped as:

```text
PortalTSRSpike Frame=...
Submitted=...
Endpoint=...
PrimaryFraction=...
ExpectedPrimary=...
Output=...
ExtractionInput=...
ObservedAA=...
Jitter=(...,...)
JitterObserved=...
CameraCut=...
CameraCuts=...
ContinuousHistoryFrames=...
CutReason=...
```

The JSON records the same state plus the current transformed player/virtual view
locations and projected portal bounds.

`observedAAMethod` is captured from the actual render-thread `FSceneView` inside
the post-processing callback rather than inferred from the requested setup.
`temporalJitterObserved` is also measured from the render-thread view matrices.

## Acceptance

PASS requires all of the following:

1. `framesSubmitted` and `lastExtractionFrame` continue advancing while moving;
2. `observedAAMethod` corresponds to `AAM_TSR` in the running UE 5.8 build;
3. `temporalJitterObserved = true` during an ordinary run;
4. with primary fraction below 1.0, the post-TSR `extractionInputSize` is the
   full output/portal target size rather than the reduced primary size;
5. `continuousHistoryFrames` grows substantially and camera cuts remain sparse;
6. the portal continues to track the transformed camera and aperture correctly;
7. no recurring assertion, stale-target use, or resource lifetime crash occurs;
8. no obvious persistent ghost trail is introduced by the secondary temporal
   history during slow camera movement.

For the default `0.67` fraction, `expectedPrimarySize` is telemetry only. Renderer
alignment/rounding can differ slightly from a naive multiplication, so the gate
does not require exact equality to that approximation. The important observable
is that the post-TSR extraction returns to full output size.

## Failure interpretation

```text
Tonemap callback never executes
    -> post-temporal extraction hook contract is wrong; do not fall back to brightness hacks

observedAAMethod != TSR
    -> secondary view AA setup is being overridden; audit SetupAntiAliasingMethod/final PP ordering

jitter never observed
    -> temporal jitter is being suppressed; audit bAllowTemporalJitter/view-state setup

ExtractionInput remains reduced primary size
    -> extraction is not actually post-upscale or screen-percentage contract is incomplete

history cuts every frame
    -> reuse/reset regression from 1B.10A
```

## Next gate after PASS

After 1B.10B passes, temporal history and fixed-fraction TSR are considered
feasible on the transformed full-view path. The next renderer work should move to
near/grazing clipping hardening and then main depth/stencil continuity. Dynamic
resolution and production GPU/VRAM budgets remain a later performance gate.

## Claim boundary

Out of scope for this gate:

```text
dynamic resolution controller
portal-bounded secondary scissor
main depth/stencil continuity
depth-correct DOF across the aperture
recursion >= 2
multi-visible-portal temporal ownership
production GPU/VRAM acceptance
Core Portal Fidelity Seal
```
