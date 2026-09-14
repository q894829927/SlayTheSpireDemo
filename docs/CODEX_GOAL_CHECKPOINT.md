# Codex Goal Checkpoint — Interior Portals

## Current resumable task — STEP 1B.5 Full View-Family Lit Renderer Spike, 2026-09-15

Branch: `portal/full-fidelity-p1`.

Current implementation HEAD at this checkpoint:

```text
aab0927cda0aa4ec250d32991e4afe9d6e903d90
```

The preceding diagnostic baseline was:

```text
85f3685220ac9a8852ab0ab590c970459a7a16ca
```

where `portal.CompositionDebugMode 3` proved that the transformed CRP path can
render coherent BaseColor through the existing portal aperture composition.
That result is recorded in `docs/InteriorPortalCRPBaseColorDiagnostic.md` and
establishes that the tested black `DepthAndBasePass + SceneColorNoAlpha` result
is not primarily an empty transformed view, total clip failure, unwritten
RenderTarget or disconnected aperture-composition problem.

### Current implementation

A new one-shot diagnostic source file is present:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalFullViewFamilySpike.cpp
```

It registers:

```text
portal.RunFullViewFamilySpike
```

The command intentionally bypasses the public CustomRenderPass render mode. In
PIE/Game, with a linked portal pair and one aperture visible, it reuses the
existing portal rigid transform and builds a standalone transformed
`FSceneView` / `FSceneViewFamilyContext`, then submits that view through:

```text
IRendererModule::BeginRenderingViewFamily
```

into a transient `PF_FloatRGBA / RTF_RGBA16f` target. The diagnostic view is a
normal game view (`bIsSceneCapture=false`), uses the logical exit plane as the
real `FSceneView::GlobalClippingPlane`, hides the portal endpoint primitives to
avoid accidental recursion, disables EyeAdaptation/MotionBlur/TemporalAA for
the first discriminating proof and requests Lumen GI/reflections through the
view post-process settings.

The command flushes rendering only because this is a one-shot evidence path,
then exports:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

Detailed scope, pass/fail split and claim boundaries are in:

```text
docs/InteriorPortalFullViewFamilySpike.md
```

### Validation state

```text
CODE ADDED
DOCUMENTED
NOT YET BUILT
NOT YET RUN
NO VISUAL PASS CLAIMED
```

No local UE build or Automation execution was available from the GitHub-only
editing environment. Do not treat the commit as compiled/validated evidence.

### Next action — USER ACTION REQUIRED

Sync `portal/full-fidelity-p1`, regenerate project files and compile the UE 5.8
Development Editor target using the standard commands in `AGENTS.md`.

Then open/run `/Game/House/L_Interior_LivingKitchen`, place/link both portals,
face one visible portal and execute:

```text
portal.RunFullViewFamilySpike
```

Inspect `Saved/AutomationReports/PortalFullViewFamilySpike.png` first and keep
the EXR/JSON as renderer evidence.

Decision:

```text
lit transformed image
    -> full-view-family lighting feasibility PASS
    -> integrate this producer with the existing pre-tonemap aperture composition path

geometry present but still unlit/BaseColor-like
    -> inspect show flags / view-family / post-process setup

black/empty output while Mode 3 remains valid
    -> inspect standalone FSceneView construction / target resolve / clip setup

compile/API failure
    -> record exact UE 5.8 limitation and move to the already-authorized renderer-private hook
```

Do not begin Step 2 or Step 3. Do not expand recursion, temporal history,
stencil/depth integration or physics until this one-layer lighting feasibility
question is answered.

## Prior checkpoint retained below

### STEP 1B.3 Main SceneColor composition boundary, 2026-09-14

Branch: `portal/full-fidelity-p1`. Prior baseline for that delivery:
`81ffb09af1945ed8f695c934bcfacd67ab29f835` (`portal: add native exit clip diagnostics`).
The existing user-owned map change in
`Content/House/L_Interior_LivingKitchen.umap` remains unrelated and must stay
uncommitted.

STEP 1B.3 result: **PARTIAL**. `CustomRenderPassCompositionSpike` was added as
an explicit opt-in backend while preserving the default `SceneCapture`
fallback, `MainViewStencilSpike` and `CustomRenderPassSpike`. It submits the
existing real transformed one-layer CRP view, copies the external target
resource identity into the immutable request, and subscribes to
`ISceneViewExtension::SubscribeToPostProcessingPass(BeforeDOF)`. The callback
reads `FPostProcessMaterialInputs::SceneColor`, imports the CRP HDR target and
returns a new RDG SceneColor before player exposure/local exposure/color
grading/tonemap.

The project-side pre-tonemap composition boundary is therefore implemented,
but the spike uses an analytic ellipse from logical projected bounds. It does
not apply main SceneDepth comparison, public main stencil, true CRP scissor or
depth-continuous wall occlusion. GPU composition and visual parity remain
manual PIE/RenderDoc evidence, not Automation claims. No Engine source was
modified, and no SceneCapture exposure/ObliqueFallback/traversal/physics path
was changed.

Changed source includes `InteriorPortalRenderer.h/.cpp`,
`InteriorPortalSystem.h/.cpp`, `SlayTheSpireDemo.Build.cs`,
`SlayTheSpireDemo.uproject`, `Shaders/InteriorPortalComposition.usf` and
`InteriorPortalTests.cpp`; docs are updated in the current execution plan and
observed-issues log. The SceneCapture backend remains the default fallback.

Validation: bundled UE 5.8 project-file generation passed; Development Editor
build passed; focused `SlayTheSpireDemo.Interior.Portals` passed **17/17** in
`Saved/AutomationReports/PortalCompositionBoundary/index.json`. The first
Automation invocation was discarded because `-run=AutomationTests` requested a
nonexistent commandlet; the corrected `UnrealEditor-Cmd` invocation without
that flag passed. The module loads at `PostConfigInit` because UE5.8 rejects
project global shader registration after shader types initialize.

Next action from that historical checkpoint was manual PIE on
`/Game/House/L_Interior_LivingKitchen` with
`RendererBackend=CustomRenderPassCompositionSpike`, `RecursionDepth=1`.
That execution path has since been superseded by the BaseColor discriminating
diagnostic and the current STEP 1B.5 full-view-family spike.
