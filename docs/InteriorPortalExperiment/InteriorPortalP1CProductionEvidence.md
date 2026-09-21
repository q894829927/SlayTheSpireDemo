# P1C — Bounded Main-Pass Production Evidence

Date: 2026-09-22  
Branch: `portal/full-fidelity-p1`  
Authority: `docs/PortalPerformanceVRAMP1Plan.md §9-10`  
Status: **P1C-1 PASS / P1C-2 GPU A-B NEXT**

## Existing accepted baseline

`docs/InteriorPortalExperiment/InteriorPortalBoundedMainPassValidation.md`
already records user-confirmed single-path bounded-main-pass visual correctness.

Current implementation retains:

~~~text
portal.BoundedMainPassScissor default = 0
portal.BoundedMainPassPaddingPixels default = 4
full-view stencil clear
SceneColor prefill for sparse output
full-view proof/debug exceptions
~~~

The current renderer already exposes sufficient runtime diagnostics through
`portal.CompositionDiagnostics=1`:

~~~text
PortalComposition BoundedPass
Requested=<0/1>
Active=<0/1>
Rect=(x0,y0)-(x1,y1)
Pixels=<bounded>/<full>
Coverage=<ratio>
Padding=<pixels>
~~~

No new instrumentation is required before the first production-evidence gate.

## P1C-1 — P1A/P1B interaction smoke

Use the current sealed production recursion policy:

~~~text
RequestedDepth = 2
portal.MinRecursionScreenCoverage 0.0025
portal.BoundedMainPassPaddingPixels 4
~~~

Choose a view where both portal endpoints are visible and recursion is present.

First establish the recursion state:

~~~text
portal.DumpMultiVisibleTSRSpike
~~~

Required:

~~~text
VisibleEndpointMask = 0x03
both visible endpoints remain valid
EffectiveDepth <= VisibleDepth <= RequestedDepth
no resource/submission failure
~~~

P1B is allowed to cut a small L1 according to the sealed 0.0025 policy; P1C
must not require artificially disabling P1B.

Then enable bounded main-pass work:

~~~text
portal.BoundedMainPassScissor 1
portal.CompositionDiagnostics 1
~~~

Observe briefly, then disable log spam:

~~~text
portal.CompositionDiagnostics 0
~~~

Required diagnostic evidence from the active frame:

~~~text
PortalComposition BoundedPass ... Requested=1 Active=1 ...
Coverage < 1 for a portal whose conservative rectangle is smaller than full view
PortalComposition DrawQueued ... BoundedScissor=1
~~~

Visual acceptance:

~~~text
both visible portals remain correct
recursive image remains correct
no black rectangle / clipped aperture
no stale pixels outside the bounded rectangle
no foreground-depth/stencil regression
no crash/assert/RDG/RHI failure
~~~

Also toggle back to:

~~~text
portal.BoundedMainPassScissor 0
~~~

at the same camera and confirm no visible semantic change.

## P1C-1 result — USER CONFIRMED PASS

User-provided PIE evidence at RequestedDepth=2 with the sealed P1B production
threshold `0.0025` shows:

~~~text
VisibleEndpointMask=0x03
SubmittedEndpointMask=0x03
PublishedEndpointMask=0x03

Endpoint 0:
VisibleDepth=2
EffectiveDepth=2
Attempted=0x03
Submitted=0x03
SubmissionCount=2
Published=0x03
Cutoff=NONE
L1 ParentCoverage=0.005130
Failure=NONE

Endpoint 1:
VisibleDepth=2
EffectiveDepth=2
Attempted=0x03
Submitted=0x03
SubmissionCount=2
Published=0x03
Cutoff=NONE
L1 ParentCoverage=0.012615
Failure=NONE
~~~

This proves the dual-visible, recursion-depth-two configuration remains healthy
with the sealed P1B policy active.

After enabling `portal.BoundedMainPassScissor=1`, render-thread diagnostics
settled to `Requested=1 Active=1` and `BoundedScissor=1` for both endpoint
composition paths. Representative bounded coverage from the accepted frame:

~~~text
recursive-view Endpoint 0: 3828 / 662860 pixels  = 0.005775
recursive-view Endpoint 1: 9027 / 662860 pixels  = 0.013618
main-view Endpoint 0:     48416 / 1008406 pixels = 0.048012
main-view Endpoint 1:    265506 / 1008406 pixels = 0.263293
~~~

The user reported the resulting image as normal. No clipping, black rectangle,
stale-pixel, recursion, foreground-depth, crash/assert, RDG or RHI regression
was reported.

The first frame immediately after the console command still reported
`Requested=0 Active=0`; the next frame and subsequent frames reported
`Requested=1 Active=1`. This is treated as ordinary console/render-thread
propagation, not a failure.

**P1C-1 = PASS.**

## P1C-2 — fixed-camera GPU A/B

Keep the exact same camera/window/settings and compare:

~~~text
portal.BoundedMainPassScissor 0
vs
portal.BoundedMainPassScissor 1
~~~

Keep P1B production default 0.0025 active for both runs.

Use the same CSV protocol as P1B:

~~~text
warm-up >= 300 frames
csvprofile frames=300
~~~

Record GPUTime average / median / P95 / P99 and the bounded-pass Coverage.

## P1C-3 — expanded visual stability

After timing, validate:

~~~text
near-screen-edge / partial offscreen portal
oblique / grazing portal
rapid camera motion / TSR jitter
dual-visible portals
recursion >= 2
~~~

Do not promote `portal.BoundedMainPassScissor` to a nonzero production default
until P1C-1 through P1C-3 are accepted.
