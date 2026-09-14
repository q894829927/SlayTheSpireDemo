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
```

Validation state:

```text
UE 5.8 PUBLIC API STATIC AUDIT PASS
NOT YET BUILT
NOT YET RUN
NO VISUAL PASS CLAIMED
```

The static audit confirmed that the UE 5.8 public API documents the principal
interfaces used by the spike (`FSceneViewInitOptions` view transform members,
`FSceneView::GlobalClippingPlane`, post-process setup methods,
`FSceneViewFamily` capture-source/additional-family fields,
`IRendererModule::BeginRenderingViewFamily`, and `FImageUtils` readback/save).
This reduces API-name risk but does not substitute for compiling the actual
project against the installed Engine.

Required next action:

1. Regenerate UE 5.8 project files.
2. Compile `SlayTheSpireDemoEditor Win64 Development`.
3. If the compiler reports an error, stop and capture the first relevant error
   block rather than changing renderer architecture speculatively.
4. If the build passes, run `/Game/House/L_Interior_LivingKitchen` in PIE/Game.
5. Place/link both portals and face one visible aperture.
6. Execute `portal.RunFullViewFamilySpike`.
7. Inspect:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

A visibly lit transformed image advances the work to integration with the
existing pre-tonemap aperture composition boundary. Compile/API failure or a
black output must be recorded before escalating to the renderer-private hook.

See `docs/InteriorPortalFullViewFamilySpike.md` for the complete claim boundary.
