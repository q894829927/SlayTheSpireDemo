# Interior Portal — STEP 1B.6 BeforeDOF Lit SceneColor Extraction Spike

Date: **2026-09-15**

Branch: **`portal/full-fidelity-p1`**

Status:

```text
IMPLEMENTED
NOT YET BUILT / RUN
NO INTEGRATED PORTAL COMPOSITION CLAIM
```

## Purpose

STEP 1B.5 proved that the transformed portal camera can drive a standalone full
`FSceneViewFamily` through `IRendererModule::BeginRenderingViewFamily` and
produce coherent lit target-space imagery. Its final exported image is strongly
exposure/post-process dominated, so it is not the correct source domain for the
Portal HDR texture.

STEP 1B.6 isolates the required source domain: **lit SceneColor at the
`BeforeDOF` post-process boundary**, before the secondary view's final display
exposure / tonemap.

## Implementation

New source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalFullViewFamilyBeforeDOFSpike.cpp
```

Command:

```text
portal.RunFullViewFamilyBeforeDOFSpike
```

The spike owns two independent `RTF_RGBA16f / PF_FloatRGBA` render targets:

```text
FinalTarget
    <- normal full FSceneViewFamily output

BeforeDofTarget
    <- temporary FSceneViewExtension callback at BeforeDOF
       copies current lit SceneColor into the external RGBA16f target
```

The extraction extension is attached only to the one-shot additional view
family. It does not modify the normal player view or the existing production /
fallback renderer paths.

The callback returns the original SceneColor unchanged after scheduling the
copy, so the secondary full view continues through its ordinary post-process
chain and still produces the final-output comparison artifact.

## Expected outputs

```text
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_Final.png
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_Final.exr
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_SceneColor.png
Saved/AutomationReports/PortalFullViewFamilyBeforeDOF_SceneColor.exr
Saved/AutomationReports/PortalFullViewFamilyBeforeDOFSpike.json
```

The JSON field:

```text
beforeDOFCallbackExecuted
```

must be `true` before any visual conclusion is made.

## Acceptance for this spike

A positive result requires all of the following:

```text
beforeDOFCallbackExecuted = true
BeforeDOF target readback succeeds
BeforeDOF output contains the same transformed target-space geometry
BeforeDOF output contains real lit shading rather than BaseColor-only content
BeforeDOF EXR is not merely the already display-processed final image copied again
```

This spike does **not** yet prove:

```text
main-view aperture composition
single final player exposure authority in the integrated path
main depth/stencil continuity
TAA/TSR history
motion vectors
recursion
full translucency/fog/decal parity
production GPU cost
```

## Next decision

If the BeforeDOF extraction is valid, the next implementation step is to use
that target as the producer for the already-proven main-player `BeforeDOF`
aperture composition boundary. The player main view then remains the only final
exposure / tone-map authority.

If the callback never executes, fix additional-family view-extension wiring.
If it executes but the target is empty, fix RDG import/copy ownership. If the
extracted image is still display-processed, move the extraction point only after
confirming the actual pass-domain evidence; do not add brightness/gamma hacks.
