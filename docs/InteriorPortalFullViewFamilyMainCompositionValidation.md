# Interior Portal — STEP 1B.7 Main-View Composition Validation

Date: **2026-09-15**

State:

```text
STEP 1B.6 BEFORE-DOF HDR EXTRACTION = PASS
STEP 1B.7 STATIC MAIN-VIEW COMPOSITION = IMPLEMENTED / RUNTIME RERUN REQUIRED
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

## Runtime correction — extraction extension was not attached to the manual family

The first successful STEP 1B.7 command run produced:

```text
status = BEFOREDOF_EXTRACTION_FAILED
beforeDOFCallbackExecuted = false
mainCompositionArmed = false
```

The secondary full renderer itself completed. The failure was specific to the
BeforeDOF callback registration for the manually constructed additional view
family.

STEP 1B.6 had explicitly attached its temporary extraction extension to the
standalone family:

```cpp
ViewFamily.ViewExtensions.Add(ExtractionExtension);
```

The initial STEP 1B.7 implementation created the same style of extension through
`FSceneViewExtensions::NewExtension`, but omitted the equivalent explicit family
attachment. A manually constructed `FSceneViewFamilyContext` does not
retroactively gather a newly created extension, so the renderer never subscribed
that extension to the secondary family's BeforeDOF pass.

The code now performs the explicit attachment before the view is submitted:

```cpp
if (ExtractionExtension.IsValid())
{
    ViewFamily.ViewExtensions.Add(ExtractionExtension.ToSharedRef());
}
```

This correction does not change the transformed camera, clip plane, HDR target,
lighting path, or main composition shader. It only restores the same extension
ownership contract that already passed in STEP 1B.6.

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

Expected report:

```text
Saved/AutomationReports/PortalFullViewFamilyMainCompositionSpike.json
```

Expected JSON gate:

```text
status = MAIN_COMPOSITION_ARMED
beforeDOFCallbackExecuted = true
mainCompositionArmed = true
```

With composition diagnostics enabled, subsequent main-frame logs should include
`PortalComposition Subscribe`, `PortalComposition Execute`, `ComposeReady` and
`DrawQueued` for the ordinary player view.

When finished, run:

```text
portal.ClearFullViewFamilyMainCompositionSpike
```

## Visual acceptance for this gate

PASS requires the visible aperture to contain the transformed target-space image
from the full renderer and for that content to participate in the main player's
post-process/exposure chain. This gate does not require perfect brightness parity.
A brightness mismatch is diagnostic evidence for the next pre-exposure ownership
step, not a reason to add an arbitrary multiplier.

Important: STEP 1B.6 showed that extracted SceneColor alpha is not a stable portal
opacity signal. The composition shader now blends only RGB through the analytic
aperture and preserves the main SceneColor alpha.

Still out of scope:

```text
per-frame full secondary producer
secondary/main pre-exposure parity
TAA/TSR and motion-vector history
main depth/stencil continuity
portal-bounded renderer scissor
recursion >= 2
full Lumen/translucency/fog acceptance
production GPU cost
Core Portal Fidelity Seal
```

If the portal content is present but too bright/dark after main tonemapping, the
next task is to measure secondary vs main pre-exposure and rebase secondary
SceneColor into the main view's pre-exposed domain before composition.
