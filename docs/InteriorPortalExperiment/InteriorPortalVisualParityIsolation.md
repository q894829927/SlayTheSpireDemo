# Interior Portal — STEP 1B.14A Visual-Parity Isolation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION
= RUNTIME EXECUTED
= PASS AS AN ISOLATION GATE
= OUTCOME B: SECONDARY FULL RENDERER / POST-PROCESS PATH REMAINS VISUALLY INCONSISTENT
= SINGLE-LAYER VISUAL PARITY NOT YET PASSED
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

`CompositionDebugMode=7` samples the same post-TSR/pre-tonemap secondary HDR
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
secondary image is intentionally presented full-screen.

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

## Runtime result — 2026-09-16

The user executed the gate with:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.StartFullViewFamilyTSRSpike
portal.StartVisualParityReference
portal.DumpVisualParityReference
```

The runtime dump reported:

```text
PortalVisualParityReference Active=1
DebugMode=7
StencilGate=0
StencilBypass=0
BoundedMainPass=0
MainDepthPropagation=0
DepthAware=0
SecondaryDepthRemap=0
PreExposureRebase=1
SecondaryPreExposure=1
```

User-provided fixed-view screenshots show the following decisive behavior:

```text
normal portal composition:
    main scene is normally exposed / bright
    transformed remote scene inside the portal is extremely dark

full-screen VisualParityReference:
    the same transformed remote scene remains extremely dark when expanded across
    the whole main viewport
    the same remote doorway / bright exterior region remains recognizable
```

Therefore the dark appearance survives after removing aperture, stencil, bounded
main pass, main-depth propagation, depth-aware composition and secondary-depth
remap from the equation.

### Classification

```text
OUTCOME B
```

The remaining mismatch is **not primarily caused by the portal aperture/composition
boundary**. The full transformed secondary image is already visually inconsistent
before it is restricted to the portal opening.

This is exactly what STEP 1B.14A was intended to classify, so the isolation gate is
accepted as **PASS**. This is not a visual-fidelity PASS.

## What this result rules out

For the observed dark-frame defect, do not spend the next pass tuning:

```text
projective aperture math
stencil gating
bounded main-pass scissor
main SceneDepth propagation
secondary depth remap
portal-edge blending
arbitrary brightness / gamma compensation
```

Those controls were disabled by the reference and the defect remained.

## Next action — STEP 1B.14B

1B.14B should now target **secondary full-view rendering parity**, not aperture
parity.

Audit the secondary `FSceneViewFamily` against the real main view in this order:

```text
1. effective EngineShowFlags relevant to lighting / post process
2. final post-process settings actually reaching the secondary FSceneView
3. exposure / eye-adaptation ownership and PreExposure semantics
4. view-family feature-level / scene / realtime / resolve configuration
5. Lumen / reflection / fog / screen-space state that differs for bAdditionalViewFamily
6. temporal state only after static exposure/post-process parity is understood
```

The first diagnostic should report main-vs-secondary values from the actual runtime
views rather than adding a corrective multiplier. The accepted pre-exposure rebase
must remain intact until a concrete semantic mismatch is demonstrated.

## Decision tree retained for reference

### A — full-screen reference looks correct

The secondary renderer is not the primary cause. Continue with portal-only parity.

### B — full-screen reference still looks inconsistent — **OBSERVED**

Do not continue performance cropping. Compare secondary `ShowFlags`, final
post-process settings and main exposure authority first. The next code change must
be driven by the observed delta, not by brightness/gamma compensation.

### C — reference is correct when static but differs while moving

Focus on temporal/screen-space state:

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

This is an isolation gate, not a visual-fidelity seal. PASS means the test can
unambiguously classify the remaining mismatch as either secondary-full-renderer or
portal-only integration.

That criterion is now satisfied.

## Remaining roadmap after classification

For **single-layer visual consistency**, the next sequence is now:

```text
1. 1B.14A classify mismatch — PASS / OUTCOME B
2. 1B.14B secondary ViewFamily / post-process / exposure parity diagnosis and fix
3. 1B.14C final static/motion/near/crossing parity matrix and seal
```

Separate from visual consistency are performance / feature gates:

```text
1B.13B/13C secondary transport/view-family bounding
production performance acceptance
recursion >= 2
```

Those are not prerequisites for fixing the current one-layer display mismatch.
