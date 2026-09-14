# Interior Portal — Full View-Family Lit Renderer Spike

Date: **2026-09-15**

Branch: **`portal/full-fidelity-p1`**

Implementation commit: **`a2204a3e307b5bef627289ffccbb094ef9c744ed`**

Status:

```text
IMPLEMENTED /
NOT YET BUILT OR RUN /
USER VALIDATION REQUIRED
```

Related evidence:

- `docs/InteriorPortalCRPBaseColorDiagnostic.md`
- `docs/InteriorPortalCurrentExecutionPlan.md`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalFullViewFamilySpike.cpp`

---

## 1. Why this spike exists

The BaseColor CRP diagnostic proved that the existing transformed portal view,
BasePass, RenderTarget and aperture composition path are alive. The remaining
blocking question is whether a transformed portal camera can obtain a **genuinely
lit image** when it is sent through a normal full UE scene renderer rather than
through the public `DepthAndBasePass` CustomRenderPass.

This spike answers only that question. It deliberately does **not** attempt to
solve recursion, stencil/depth aperture ownership, production temporal history,
exposure parity or final renderer integration in the same change.

---

## 2. Renderer path under test

The command uses a standalone ordinary view family:

```text
player camera + linked portal pair
    -> existing portal rigid transform
    -> transformed FSceneView
    -> standalone FSceneViewFamilyContext
    -> IRendererModule::BeginRenderingViewFamily
    -> RGBA16f UTextureRenderTarget2D
    -> blocking readback
    -> PNG + EXR evidence
```

Important differences from the previous CRP spike:

```text
FSceneView::bIsSceneCapture = false
normal Lit view family
full renderer entry point through IRendererModule
real FSceneView::GlobalClippingPlane = logical exit plane
portal actors hidden from this one-shot view to avoid accidental recursion
```

The spike disables EyeAdaptation, MotionBlur, TemporalAA and screen percentage
for the first discriminating run. It explicitly requests Lumen GI and Lumen
reflections in the view's final post-process settings. This is intentional: the
first run is about establishing that lighting stages execute at all, not about
solving temporal/exposure parity.

The renderer target is `PF_FloatRGBA / RTF_RGBA16f`.

---

## 3. How to run

First sync and build the branch normally. Then run the map in PIE/Game with both
portals placed and linked. Make sure at least one portal aperture is visible in
the player camera.

Execute:

```text
portal.RunFullViewFamilySpike
```

The command is intentionally one-shot and blocking. It flushes the render thread
before readback so the evidence belongs to the exact transformed view submitted
by the command.

Expected outputs:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

The JSON records the transformed endpoint, view location/rotation, logical exit
clip plane, projected portal bounds, target size and whether readback/export
succeeded.

---

## 4. What counts as a positive result

The first gate passes if the exported image contains coherent target-space
geometry **with real lighting information**, for example:

```text
material colors are visible
+ direct-light response is visible
+ lit/shadowed regions differ
+ scene is not the flat BaseColor diagnostic image
```

Lumen/reflection content is useful additional evidence, but the first image does
not need to prove every full-fidelity render feature.

A positive result means:

```text
portal transform / projection can drive a normal full scene view
full renderer lighting stages can produce portal-space image content
public DepthAndBasePass CRP was the limiting renderer stage, not the portal camera
```

The next step would then integrate this full-view producer with the already
validated pre-tonemap portal composition boundary, before addressing
stencil/depth/scissor and temporal/exposure parity.

---

## 5. What this spike does not prove

Even if the exported image is lit, do not claim any of the following yet:

```text
portal aperture composition parity
main depth/stencil continuity
true CRP/main-view scissor
near/grazing/edge acceptance
exposure parity with direct player view
single final exposure authority in the integrated path
TAA/TSR history quality
motion-vector correctness
recursion >= 2
translucency/fog/decal parity
production GPU cost
Core Portal Fidelity Seal
```

The output is an **offscreen renderer feasibility artifact**, not the production
Portal renderer.

---

## 6. Failure split

If the command writes a coherent lit image:

```text
FULL VIEW-FAMILY LIT FEASIBILITY = PASS
-> integrate this producer with the Portal composition path
```

If the output contains geometry but is still BaseColor-like/unlit:

```text
FULL VIEW-FAMILY SUBMISSION WORKS
LIGHTING STILL MISSING
-> inspect show flags / view-family/view-state setup before Engine-private escalation
```

If the output is black or empty while Mode 3 BaseColor remains valid:

```text
new full-view-family construction is wrong
-> inspect FSceneViewInitOptions, target resolve, clipping and render-family setup
```

If project code cannot compile or UE rejects the standalone view-family path:

```text
PUBLIC FULL VIEW-FAMILY SPIKE BLOCKED
-> record the exact API/renderer limitation
-> proceed to the already-authorized renderer-private hook decision
```

Do not fall back to brightness multipliers or add more CRP output modes as a
substitute for this decision.
