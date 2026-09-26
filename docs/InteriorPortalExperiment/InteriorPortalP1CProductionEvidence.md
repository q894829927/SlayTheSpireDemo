# P1C — Bounded Main-Pass Production Evidence

Updated: 2026-09-26

Branch: `portal/full-fidelity-p1`  
Authority: `docs/PortalPerformanceVRAMP1Plan.md §9-10`  
Status: **P1C COMPLETE / VALIDATED / SEALED — 2026-09-26; scissor default remains 0**

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

## P1C-2 result — USER CAPTURED COMPLETE / NO PERFORMANCE WIN

The user provided two 300-frame CSV captures in the prescribed order:

~~~text
Profile(20260922_011453).csv -> portal.BoundedMainPassScissor=0
Profile(20260922_011526).csv -> portal.BoundedMainPassScissor=1
~~~

Both captures contain exactly 300 valid `GPUTime` samples. The camera is
identical across both captures:

~~~text
View/Pos = (1163.9623, 609.1017, 120.1500)
View/Forward = (0.8914, 0.4526, -0.023018)
View/Speed = 0
~~~

Measured GPU timing:

| Metric | Scissor 0 | Scissor 1 | Delta | Change |
|---|---:|---:|---:|---:|
| GPU Average | 38.373 ms | 39.014 ms | +0.641 ms | +1.67% |
| GPU Median | 38.224 ms | 38.867 ms | +0.644 ms | +1.68% |
| GPU P95 | 39.433 ms | 40.280 ms | +0.847 ms | +2.15% |
| GPU P99 | 41.609 ms | 42.311 ms | +0.701 ms | +1.69% |

Supporting frame/thread timing:

~~~text
FrameTime average:        42.319 -> 43.536 ms (+1.217 ms / +2.88%)
RenderThreadTime average: 42.304 -> 43.526 ms (+1.222 ms / +2.89%)
GameThreadTime average:    7.833 ->  8.738 ms (+0.904 ms)
RHIThreadTime average:    12.719 -> 14.127 ms (+1.409 ms)
~~~

The draw-call count is effectively unchanged:

~~~text
RHI/DrawCalls average:
2199.70 -> 2199.35
~~~

Therefore this fixed production camera does **not** show a GPU benefit from the
current bounded-main-pass implementation. It shows a small but consistent
regression instead.

This is compatible with the current implementation contract: enabling bounded
composition turns the output into a sparse pass and, when the post-process
output texture differs from incoming SceneColor, performs a full SceneColor
prefill copy before the smaller bounded draws. The saved raster work therefore
does not automatically imply lower total GPU cost.

Do not promote `portal.BoundedMainPassScissor` to the production default from
this evidence. The code default remains `0`.

**P1C-2 measurement is complete. The result is negative for performance at the
validated camera, not a validation failure.**

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

## P1C-3 execution — 2026-09-26

Verified source HEAD: `78057a34022e0f979be887a9ad7f97038c3350bf`.
No renderer, gameplay, asset or build configuration change was made for this
evidence pass. Existing P1C-1/P1C-2 acceptance is reused.

Actual UE 5.8 D3D12 floating PIE ran on
`/Game/House/L_Interior_LivingKitchen` with these fixed conditions:

~~~text
portal.FullFidelityPingPong = 0 (existing code default / fallback producer)
RequestedDepth = 2
portal.MinRecursionScreenCoverage = 0.0025
portal.BoundedMainPassPaddingPixels = 4
portal.BoundedMainPassScissor = 0 vs 1
PrimaryResolutionFraction = 0.67
Secondary observed AA method = 4 (TSR), TemporalJitterObserved = true
Requested floating window = 1280 x 720
Actual screenshot / parent-view extent = 1280 x 723
Main composition SceneRect = 1278 x 722
~~~

The measured extents, rather than the requested window size, define this
fixture. This pass does not claim Ping-Pong=1 coverage or performance results.
Portal placement, map contents and renderer policies were left unchanged.
Stationary poses warmed for 40 editor ticks before ordinary `Shot` captures;
HighResShot, temporal-history resets and graphics overrides were not used.

### Stationary A/B observations

Seven poses produced 14 screenshots and 14 renderer JSON reports. All A/B
pairs were visually inspected. Position below is the camera eye in centimeters;
roll is zero. Endpoint depth values are EffectiveDepth for endpoints 0 / 1.

| Capture prefix | Eye position | Yaw / pitch | Actual scenario | Depths, both A/B runs |
|---|---|---|---|---|
| `dual_partial` | (1163.9623, 609.1017, 120.15) | 26.918 / -1.319 | Both endpoints; near aperture partly off right edge | 1 / 2 |
| `partial_left` | Same | 68 / -1.319 | Both apertures inside view; centered reference despite historical capture name | 2 / 2 |
| `partial_right` | Same | -10 / -1.319 | Both apertures outside view; no residual portal rectangle | 0 / 0 |
| `oblique` | (1100, 790, 120.15) | 28 / -4 | Oblique near aperture | 1 / 2 |
| `grazing` | (900, 850, 120.15) | 6 / -4 | Near aperture almost edge-on; far aperture still visible | 1 / 1 |
| `near` | (1300, 730, 120.15) | 90 / -4 | Large aperture clipped vertically; foreground weapon overlaps | 2 / 2 |
| `left_edge` | (1163.9623, 609.1017, 120.15) | 110 / -1.319 | Near aperture partly off left edge | 2 / 0 |

No scissor-dependent black rectangle, extra aperture truncation, vertical slit,
stretching or pixels left outside the aperture was observed in these captures.
Foreground weapon overlap remained visually consistent. The thin existing
portal/base seam is present in both paths; this pass does not classify it as a
new scissor defect or claim to repair portal placement.

Every submitted layer in the stationary reports has
`SubmissionFailureReason=NONE`. Visible/submitted/published endpoint masks
match in each report; EffectiveDepth and submitted layer masks match across
each A/B pair. Depth-two submission is actually exercised, rather than inferred
from RequestedDepth. Grazing views legitimately reduce recursive workload.
Diagnostics contain active bounded main-pass draws at the measured main-view
extent, with the configured four-pixel padding.

### Camera-motion samples and remaining gate

A native PIE camera sweep ran for 240 editor ticks (7.72 seconds of reported
tick time), with scissor=1 and unchanged production recursion policy:

~~~text
Yaw(n) = 68 + 83 * sin(2*pi*n/60), n = 0..239
Pitch(n) = -1.319 + 12 * sin(2*pi*n/47)
~~~

Twelve ordinary screenshots and renderer reports were sampled every 20 ticks.
All sampled images were inspected, including dual-visible recursive frames
and views after the apertures move offscreen. No black rectangle or portal
residue was observed in the samples. They contain motion blur; they do not
establish that all intermediate frames are free of flashes, temporal trails or
TSR edge instability. The execution log contains no fatal/assert/ensure,
GPU-crash, out-of-video-memory or renderer/D3D12 error matching the checked
patterns. This is a functional observation, not a GPU benchmark.

**Continuous-motion acceptance procedure — completed by the user below.**
In `L_Interior_LivingKitchen`, use RequestedDepth=2, the existing producer
mode, production coverage threshold 0.0025, padding=4 and scissor=1. Starting
near the reference pose, rapidly turn left/right and up/down so an aperture
crosses both screen edges, then return to a dual-visible view. Keep the weapon
visible and approach/leave the aperture at an oblique angle. Compare once with
scissor=0. Required observation: no short-lived black/white rectangle, stale
image outside the aperture, new edge flicker, extra clipping or foreground
occlusion change attributable to scissor. Provide a PASS/FAIL description and,
if a defect appears, a short video with the view/settings. Restore scissor=0
afterward. Do not repeat the sealed P1C-2 GPU measurement.

The sampled pass alone did not close P1C. The subsequent user confirmation below
closes the remaining gate. Keep the production scissor default at 0; the existing
+1.67% timing result still does not justify enabling it by default.

### Local evidence and cleanup

~~~text
Saved/PortalP1C3/{capture-prefix}_S{0,1}.{png,json}
Saved/PortalP1C3/static-complete.json (first six camera A/B pairs)
Saved/PortalP1C3/static-summary.json (all seven A/B pairs)
Saved/PortalP1C3/main-bounded-diagnostics.json
Saved/PortalP1C3/motion_{000,020,...,220}.{png,json}
Saved/PortalP1C3/motion-complete.json (240 camera samples)
Saved/PortalP1C3/{static,motion}-matrix.py (local capture commands)
Saved/PortalP1C3/cleanup.json
Saved/PortalP1C3/StopPIE.json
Saved/Logs/PortalP1C3.log
~~~

These are local, ignored validation artifacts, not files to commit. Temporary
capture-script setup errors were corrected before the recorded matrix completed;
stale `error.txt` / `static-error.txt` are not acceptance evidence.
PIE was stopped; scissor/diagnostic CVars and temporary editor throttle/window
preferences were restored. No C++ build or Automation rerun was needed because
no executable project source changed.

The user's pre-existing modified map remains modified and byte-identical to
the start of this pass. Its SHA-256 before and after is:
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.

## Final user acceptance — 2026-09-26

In response to the explicit question about rapid turns, screen-edge crossings
and scissor=1 vs 0 without new flicker/residue, the user confirmed:
**“已完成，画面正常”**. This is the continuous-motion manual evidence; it is
separate from the earlier agent-inspected samples. No new video, GPU measurement
or additional producer-mode acceptance is inferred from this response.

P1C-1, P1C-2 and P1C-3 are now complete for the specified scope. **P1C is
COMPLETE / VALIDATED / SEALED**, with `portal.BoundedMainPassScissor` still
default-disabled. Alongside the already sealed P1A/P1B, this closes Performance
/ VRAM P1 and permits the next Physics P1 migration stage.
