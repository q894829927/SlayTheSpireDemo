# Interior Portal — CRP BaseColor Diagnostic Result

Date: **2026-09-15**

Branch: **`portal/full-fidelity-p1`**

Diagnostic baseline HEAD: **`85f3685220ac9a8852ab0ab590c970459a7a16ca`**

Related commits:

```text
8b0b39d7703cf7b4e564405ddc9849f522a10963
portal: route BaseColor diagnostic through aperture

85f3685220ac9a8852ab0ab590c970459a7a16ca
portal: add BaseColor CRP diagnostic mode
```

Related authority:

- `docs/InteriorPortalCurrentExecutionPlan.md`
- `docs/InteriorPortalFullFidelityImplementationPlan.md`
- `docs/InteriorPortalObservedIssues.md`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalRenderer.cpp`
- `Shaders/InteriorPortalComposition.usf`

---

## 1. Purpose

This diagnostic was added to distinguish two very different failure classes in the current public CustomRenderPass composition spike:

```text
A. transformed portal view / geometry / RenderTarget / composition is broken

vs

B. the transformed geometry path works, but the current CRP render mode does not produce the fully lit SceneColor required by the portal visual target
```

The diagnostic intentionally keeps the same transformed virtual camera, projection, CRP submission, RenderTarget ownership and `BeforeDOF` composition path. Only the requested CRP render output changes in Mode 3.

---

## 2. Diagnostic modes

`portal.CompositionDebugMode` currently has the following contract:

```text
0 -> CRP output SceneColorNoAlpha, normal aperture composition
1 -> force solid magenta inside the analytic portal aperture
2 -> force full-screen magenta
3 -> CRP output BaseColor, sampled through the normal portal aperture
```

Mode 3 switches the CustomRenderPass output from:

```cpp
FCustomRenderPassBase::ERenderOutput::SceneColorNoAlpha
```

to:

```cpp
FCustomRenderPassBase::ERenderOutput::BaseColor
```

while the render mode remains:

```cpp
FCustomRenderPassBase::ERenderMode::DepthAndBasePass
```

The composition shader explicitly excludes Mode 3 from the Mode 2 full-screen-magenta branch and from the Mode 1 aperture-magenta branch, so Mode 3 samples the real CRP BaseColor texture through the existing analytic aperture mask.

---

## 3. User-observed Mode 3 result

The 2026-09-15 Mode 3 `CompositionA.png` result is **positive and discriminating**.

Inside the portal aperture the image contains clearly recognizable unlit/base-pass material information rather than a uniform black target:

```text
gray floor / lower surface
olive-green horizontal surface / band
center character with multiple material colors
right-side object with blue/cyan material region
```

The main view outside the portal remains the normal lit scene, and the BaseColor result remains constrained by the existing portal aperture rather than replacing the whole frame.

The upper region of the portal output remains black. In this diagnostic that is not sufficient evidence of a broken portal path: BaseColor only represents surfaces that participate in the relevant base-pass output, and background/sky/no-surface regions can legitimately contain no useful BaseColor signal.

---

## 4. What this result proves

The Mode 3 result is accepted as evidence for the following bounded claims:

```text
transformed virtual view reaches target-space geometry      PROVEN FOR THIS VIEW
CRP Depth/BasePass submits visible geometry                 PROVEN
BaseColor output is written                                 PROVEN
portal RenderTarget receives non-black scene data           PROVEN
PortalTexture is sampled by the composition shader          PROVEN
analytic aperture composition is active                     PROVEN FOR THIS VIEW
Mode 2 routing no longer intercepts Mode 3                  PROVEN BY RESULT + CODE
"all geometry clipped away" as the cause of black Mode 0   RULED OUT FOR THIS VIEW
"portal RT never receives scene data" as the cause          RULED OUT
"composition samples only an empty target" as the cause     RULED OUT
```

This materially narrows the current failure boundary. The previous black `SceneColorNoAlpha` portal result cannot be explained by a completely dead transformed-view, BasePass, RenderTarget or aperture-composition chain when the same chain produces meaningful BaseColor under Mode 3.

---

## 5. What this result does not prove

Do **not** promote the diagnostic into claims that were not tested.

Mode 3 does not yet prove:

```text
final transformed-camera parity at every angle
near / edge / grazing-angle correctness
main-view depth continuity
main stencil aperture ownership
true CRP scissor application
full deferred direct lighting
shadow parity
Lumen GI parity
Lumen / other reflection parity
translucency / fog / decal parity
motion-vector correctness
TAA / TSR history quality
exposure parity
recursion >= 2
production renderer acceptance
```

The transformed camera is proven sufficient to see coherent target-space geometry in this tested view; the complete parallax / edge / grazing acceptance matrix remains open.

---

## 6. Renderer conclusion

The current CustomRenderPass is constructed with:

```text
RenderMode  = DepthAndBasePass
Output      = SceneColorNoAlpha   (normal mode)
```

and Mode 3 changes only the output to `BaseColor` while preserving `DepthAndBasePass`.

The observed combination is:

```text
Mode 0 SceneColorNoAlpha -> portal scene content remains black / not usable as the required lit portal view
Mode 3 BaseColor         -> coherent target geometry and material/base-pass colors are visible
```

Therefore the current evidence isolates the problem **after / beyond the geometry + BasePass portion of this CRP path**. The present `DepthAndBasePass` CustomRenderPass configuration is not sufficient to produce the required fully lit HDR portal image.

This is a narrower and safer conclusion than claiming that every UE 5.8 CustomRenderPass configuration is fundamentally incapable of lighting. What is now ruled out is continued debugging of this exact black result as if it were primarily a portal transform, empty RenderTarget, aperture mask or total clipping failure.

The project should not continue adding material/exposure gains or additional CRP color-output modes in an attempt to turn the current `DepthAndBasePass` result into production full-fidelity lighting without a new renderer-level hypothesis.

---

## 7. Next renderer step

The next authorized Step 1 renderer experiment should move to a **full transformed secondary/sub-view renderer path** (renderer-private / full SceneView execution as required), while preserving the already validated project-side boundaries where useful:

```text
existing portal transform + projection
    -> transformed secondary scene view
    -> full required renderer stages for a lit HDR result
    -> portal HDR target or equivalent main-view target
    -> existing pre-tonemap / BeforeDOF composition boundary where still applicable
    -> player remains final exposure / local exposure / tone-map authority
```

Start with the smallest discriminating spike:

```text
one visible portal
one transformed view
RecursionDepth = 1
no attempt to solve the full recursion/temporal matrix yet
produce a genuinely lit portal image
preserve the current aperture composition path for comparison
```

Only after that succeeds should the work expand to main depth/stencil continuity, true scissor/aperture geometry, independent temporal/Lumen histories, recursion, exposure parity and the full Step 1 renderer acceptance matrix.

`portal.CompositionDebugMode 3` should remain available as a permanent layer diagnostic. If a future full-render path becomes black, Mode 3 provides a fast split:

```text
Mode 3 still shows BaseColor -> geometry/BasePass/RT/composition likely alive; inspect later renderer stages
Mode 3 also becomes black    -> re-open view transform / clipping / geometry / RT / composition diagnostics
```

---

## 8. Validation classification

This record contains **manual visual evidence supplied from the actual Mode 3 run** plus the already-reviewed code contract for the diagnostic modes.

It is not a Core visual acceptance pass and does not replace the unified Step 1 matrix.

Current classification:

```text
CRP transformed geometry / BasePass feasibility      PASS FOR TESTED VIEW
CRP BaseColor -> portal aperture composition         PASS FOR TESTED VIEW
current DepthAndBasePass lit SceneColor target       INSUFFICIENT / BLOCKING
full transformed lit renderer                        NEXT FEASIBILITY STEP
Core Portal Fidelity Seal                            OPEN
Full Physics Fidelity Seal                           DEFERRED
```
