# Interior Portal — STEP 1B.7 Main-View Composition Validation

Date: **2026-09-15**

State:

```text
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = PASS
STEP 1B.8 SECONDARY->MAIN PRE-EXPOSURE REBASE = NEXT
```

Implementation commits:

```text
d7c79ff09c236a42cfa747fa52f76ef95c74970c
docs(portal): record BeforeDOF extraction pass

0525c63eb874d038654354693425c259b7197be5
portal: preserve main alpha during HDR composition

2997842ab593ddc236182280ea5ba4cc115dd269
portal: add full-view main composition spike

ebaf72defd7a52efb0825ea2d5fbbc0568841b3f
portal: isolate full view spike unity symbols

e833e9bff0003a6328ea01d8fe25e42229ad021c
portal: isolate BeforeDOF spike unity symbols

39481858e26e40898abb3ed93e17d62d67d48e04
portal: isolate main composition spike unity symbols

bd627497d62d5df207ac597f143e56f0faf045b5
portal: attach 1B.7 extraction extension to standalone view family
```

## Purpose

This gate does not introduce a per-frame full secondary renderer yet. It freezes
one full-renderer transformed view, extracts its linear HDR SceneColor at
BeforeDOF into the endpoint's persistent RGBA16F target, and then feeds that
static texture through the existing player/main-view BeforeDOF analytic aperture
compositor.

The test isolates the remaining domain question:

```text
Can the validated secondary pre-tonemap HDR SceneColor survive composition into
main pre-tonemap SceneColor and then receive the player's normal final exposure /
tonemap as part of the main frame?
```

## Compile correction — Unity Build helper collision

The first build after adding the STEP 1B.7 source failed with `C2084`, followed by
`C2440` / `C2264` cascade errors. UnrealBuildTool had grouped multiple portal spike
`.cpp` files into the same generated Unity translation unit. Each spike used an
unnamed namespace and repeated helper names such as:

```text
FindPortalSpikeWorld
FindPortalSystem
IsFiniteTransform
HidePortalPrimitives
```

Multiple unnamed-namespace declarations in one translation unit denote the same
internal namespace, so those helpers became duplicate definitions once UBT
combined the files.

The fix does **not** disable Unity Build globally. Each spike source now owns a
file-unique named private namespace:

```text
InteriorPortalFullViewFamilySpikePrivate
InteriorPortalFullViewFamilyBeforeDOFSpikePrivate
InteriorPortalFullViewFamilyMainCompositionSpikePrivate
```

This is a build-structure correction only. It does not alter the virtual camera,
BeforeDOF extraction, HDR target, clipping plane, main composition, or renderer
claim boundary.

## Runtime correction — extraction extension was not attached to the standalone family

The first STEP 1B.7 runtime attempt completed the secondary full renderer but
reported:

```text
status = BEFOREDOF_EXTRACTION_FAILED
beforeDOFCallbackExecuted = false
mainCompositionArmed = false
```

The temporary extraction extension had been created through
`FSceneViewExtensions::NewExtension`, but unlike the already proven STEP 1B.6
path it had not been explicitly appended to the manually constructed
`ViewFamily.ViewExtensions` array. Because this is a standalone additional view
family, the newly created extension was not automatically part of that family.

The fix explicitly attaches the extension to the family before renderer
submission.

## Runtime result — static main-view composition passed

The rerun after the attachment fix reports:

```text
status = MAIN_COMPOSITION_ARMED
targetSize = 1749 x 879
beforeDOFCallbackExecuted = true
mainCompositionArmed = true
```

The ordinary player-view diagnostics also repeatedly reached:

```text
PortalComposition Subscribe
PortalComposition Execute
PortalComposition ComposeReady
PortalComposition DrawQueued
```

The visible portal aperture contains transformed target-space content from the
full secondary renderer. Therefore the complete static chain is now proven:

```text
full transformed secondary FSceneViewFamily
    -> secondary BeforeDOF lit HDR SceneColor extraction
    -> persistent portal RGBA16F target
    -> player/main BeforeDOF analytic aperture composition
    -> main frame continues through final post process
```

This is a STEP 1B.7 PASS for composition feasibility.

The screenshot is still visibly over-exposed inside the portal while the main
scene remains normally exposed. This is now a domain mismatch rather than a
missing renderer/composition path. Do not add arbitrary brightness, gamma, or
tone-map compensation.

## Architectural consequence — STEP 1B.8

UE pre-exposure scales SceneColor before later post processing. A portal texture
captured from one view must be converted from the secondary view's pre-exposed
domain into the main view's pre-exposed domain before RGB composition.

The required relation is:

```text
PortalRGB_in_main_domain = PortalRGB_from_secondary
                         * MainPreExposure / SecondaryPreExposure
```

The next spike should therefore:

1. measure the secondary full view's current PreExposure from its independent
   `FSceneViewStateInterface` after the renderer submission;
2. read the main player's current PreExposure from the main `FSceneView` state in
   the BeforeDOF composition callback;
3. log both values and the ratio;
4. apply only that ratio to portal RGB before the analytic aperture blend;
5. keep the main SceneColor alpha unchanged;
6. leave the old CRP/BaseColor debug modes unscaled so prior diagnostics remain
   interpretable.

This is an exposure-domain conversion, not an artistic brightness correction.
The player/main view remains the only final exposure + tone-map authority.

## Controlled setup

Use the ordinary portal map in PIE/Game with both portals linked. For this spike,
set:

```text
RendererBackend = SceneCapture
```

This prevents the older CustomRenderPass/MainView spike backends from registering
a competing main-view composition pass. The SceneCapture portal surface may still
exist underneath, but the STEP 1B.7 post-process aperture draw owns the diagnostic
ellipse in SceneColor.

Keep one portal visible and keep the camera still after running the command. The
request contains one projected-bounds snapshot; this is intentionally a static
one-shot proof, not a tracking renderer.

Optional but recommended before running:

```text
portal.CompositionDiagnostics 1
portal.CompositionDebugMode 0
```

Run:

```text
portal.RunFullViewFamilyMainCompositionSpike
```

When finished, run:

```text
portal.ClearFullViewFamilyMainCompositionSpike
```

## Claim boundary

Still out of scope:

```text
per-frame full secondary producer
TAA/TSR and motion-vector history
main depth/stencil continuity
portal-bounded renderer scissor
recursion >= 2
full Lumen/translucency/fog acceptance
production GPU cost
Core Portal Fidelity Seal
```
