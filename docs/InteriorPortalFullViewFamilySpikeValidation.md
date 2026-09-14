# Interior Portal — STEP 1B.5 Validation Handoff

Date: **2026-09-15**

This file is intentionally small and current. It exists so the full-view-family
spike can be resumed without rewriting the historical global checkpoint.

Current branch work:

```text
a2204a3e307b5bef627289ffccbb094ef9c744ed
portal: add full view-family lit renderer spike

aab0927cda0aa4ec250d32991e4afe9d6e903d90
docs(portal): document full view-family lit spike

d3eb17c0c61522868634084d55d34fbff18c4dd1
portal: harden full view-family spike lifetime

fd5443d6e8bc0143e19c9f8fac9fba6bcb73a09d
docs(portal): update full view-family spike audit

9eda3aa84a487b3037aa74aaf2d156bfaede50db
portal: fix primitive id access in full view spike
```

Validation state:

```text
UE 5.8 PUBLIC API STATIC AUDIT PASS
FIRST REAL BUILD ATTEMPT: FAILED AT HiddenPrimitives COMPONENT ID ACCESS
THAT COMPILE ERROR IS FIXED IN 9eda3aa84a487b3037aa74aaf2d156bfaede50db
BUILD RERUN REQUIRED
SPIKE NOT YET RUN
NO VISUAL PASS CLAIMED
```

The first real UE 5.8 compile found this source-level mismatch:

```text
InteriorPortalFullViewFamilySpike.cpp(82,53):
error C2039: 'ComponentId' is not a member of 'UPrimitiveComponent'
```

The spike had attempted to populate `FSceneViewInitOptions::HiddenPrimitives`
with a non-public/nonexistent member:

```cpp
ViewInitOptions.HiddenPrimitives.Add(Primitive->ComponentId);
```

UE 5.8 exposes the renderer identity through the public
`UPrimitiveComponent::GetPrimitiveSceneId()` accessor. The code now uses:

```cpp
ViewInitOptions.HiddenPrimitives.Add(Primitive->GetPrimitiveSceneId());
```

This is a narrow compile/API correction only. It does not change the transformed
view, clipping, renderer entry point, target format or claim boundary.

The earlier static audit confirmed that the UE 5.8 public API documents the
principal interfaces used by the spike (`FSceneViewInitOptions` view transform
members, `FSceneView::GlobalClippingPlane`, post-process setup methods,
`FSceneViewFamily` capture-source/additional-family fields,
`IRendererModule::BeginRenderingViewFamily`, and `FImageUtils` readback/save).
The build must continue to be treated as the actual authority for any remaining
signature/member differences in the installed UE 5.8 tree.

Required next action:

1. Pull commit `9eda3aa84a487b3037aa74aaf2d156bfaede50db` or later.
2. Regenerate UE 5.8 project files if needed.
3. Compile `SlayTheSpireDemoEditor Win64 Development` again.
4. If another compiler error appears, stop and capture the first relevant error
   block rather than changing renderer architecture speculatively.
5. If the build passes, run `/Game/House/L_Interior_LivingKitchen` in PIE/Game.
6. Place/link both portals and face one visible aperture.
7. Execute `portal.RunFullViewFamilySpike`.
8. Inspect:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

A visibly lit transformed image advances the work to integration with the
existing pre-tonemap aperture composition boundary. Compile/API failure or a
black output must be recorded before escalating to the renderer-private hook.

See `docs/InteriorPortalFullViewFamilySpike.md` for the complete claim boundary.
