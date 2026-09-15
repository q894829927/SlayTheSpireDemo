# Interior Portal — STEP 1B.14A Visual-Parity Isolation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION
= IMPLEMENTED
= NOT YET BUILT OR RUN
```

## Why this step comes before more performance work

STEP 1B.13A bounded main-pass scissoring is accepted. The originally planned
follow-up, 1B.13B, reduces secondary color/depth transport cost and eventually
transport allocation. That work is useful for performance, but it is not expected
to explain a remaining visual mismatch by itself.

The current user priority is eliminating the remaining portal display inconsistency.
Therefore this gate isolates the full transformed secondary color path from all
portal-only aperture/depth/stencil/scissor behavior before more resource cropping.

## Goal

Answer one binary question:

```text
Does the transformed secondary full FSceneViewFamily itself already look correct
when shown full-screen through the main view's accepted post-process/exposure path?
```

If YES:

```text
secondary Lumen / shadows / reflections / TSR / HDR production is broadly sound
remaining mismatch is portal-only integration or boundary behavior
```

If NO:

```text
remaining mismatch is upstream in secondary view-family configuration,
post-process ownership, exposure semantics, or temporal/screen-space rendering
```

This prevents more depth/stencil/performance work from hiding the actual visual
root cause.

## Implementation

Shader:

```text
Shaders/InteriorPortalComposition.usf
```

`CompositionDebugMode=7` now samples the same post-TSR/pre-tonemap secondary HDR
texture across the full main view and applies the already accepted
`PortalExposureScale` before returning it to the normal main post-process chain.

Control source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalVisualParityValidation.cpp
```

Commands:

```text
portal.StartVisualParityReference
portal.DumpVisualParityReference
portal.StopVisualParityReference
```

`StartVisualParityReference` saves and temporarily disables portal-only gates:

```text
portal.StencilCompositionBypassShaderAperture = 0
portal.StencilGatedComposition = 0
portal.BoundedMainPassScissor = 0
portal.MainDepthPropagation = 0
portal.DepthAwareComposition = 0
portal.SecondaryDepthRemap = 0
portal.CompositionDebugMode = 7
```

It intentionally does **not** stop the full-view TSR producer, change the persistent
secondary `ViewState`, or override `portal.PreExposureRebase` /
`portal.SecondaryPreExposure`. Those remain owned by the already accepted producer.

## Runtime procedure

Close Unreal Editor, pull the branch and rebuild because this step adds a C++ file
and changes a Global Shader.

Enter PIE and establish the accepted secondary producer:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
```

Use the normal accepted portal path first and capture a screenshot of the remaining
visual mismatch.

Then, without moving the camera:

```text
portal.StartVisualParityReference
portal.DumpVisualParityReference
```

The portal aperture disappears as a special region because the transformed
secondary image is now intentionally presented full-screen.

Expected log:

```text
PortalVisualParityReference Active=1 DebugMode=7
StencilGate=0
BoundedMainPass=0
MainDepthPropagation=0
DepthAware=0
SecondaryDepthRemap=0
PreExposureRebase=1
```

## What to inspect

Ignore portal frame geometry while the reference is active. Inspect the transformed
remote scene itself:

```text
lighting / GI
shadow contrast
reflection response
fog / atmospheric contribution
material color
brightness / exposure
TSR stability while moving slowly
fine geometry / edge stability
```

The reference should look like a coherent full rendered scene, not a dimmed,
washed-out, differently graded, temporally unstable or partially missing version.

## Decision tree

### A — full-screen reference looks correct

The secondary renderer is not the primary cause. Continue with portal-only parity:

```text
1B.14B aperture/boundary parity isolation
1B.14C moving/near/crossing final fidelity matrix
```

Likely remaining fixes would be in composition edge policy, downstream main-view
consumers, or portal presentation geometry rather than Lumen/TSR production.

### B — full-screen reference still looks inconsistent

Do not continue performance cropping. Compare secondary `ShowFlags`, final
post-process settings and main exposure authority first. The next code change must
be driven by the observed delta, not by brightness/gamma compensation.

### C — reference is correct when static but differs while moving

Focus next on temporal/screen-space state:

```text
TSR history
velocity / motion-vector ownership
camera-cut semantics
screen-space reflection / history behavior
```

Do not modify exposure to mask a temporal defect.

## Cleanup

```text
portal.StopVisualParityReference
```

This restores the composition controls captured at `Start`.

## PASS criteria

This is an isolation gate, not yet a visual-fidelity seal. PASS means the test can
unambiguously classify the remaining mismatch as either secondary-full-renderer or
portal-only integration.

## Remaining roadmap after classification

For **single-layer visual consistency**, the expected remaining work is approximately:

```text
1. 1B.14A classify the mismatch (this gate)
2. 1B.14B fix the classified visual delta
3. 1B.14C run the final static/motion/near/crossing parity matrix and seal it
```

If 1B.14A shows that the secondary reference is already correct, step 2 may be one
small composition fix. If it exposes a renderer-history or post-process mismatch,
step 2 may split into two implementation passes.

Separate from visual consistency are performance / feature gates:

```text
1B.13B/13C secondary transport/view-family bounding
production performance acceptance
recursion >= 2
```

Those are not prerequisites for fixing the current one-layer display mismatch.
