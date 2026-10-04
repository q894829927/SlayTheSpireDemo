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

7f08c23429dba76158bc3749e349010095ffdd18
portal: load renderer module explicitly for full view spike

6d16b7e29f6e512ab8afb1ee58755188d04eb8ce
portal: install screen percentage driver for full view spike
```

Validation state:

```text
UE 5.8 PUBLIC API STATIC AUDIT PASS
REAL BUILD ATTEMPT 1: FAILED AT HiddenPrimitives COMPONENT ID ACCESS
FIXED IN 9eda3aa84a487b3037aa74aaf2d156bfaede50db
REAL BUILD ATTEMPT 2: FAILED BECAUSE GetRendererModule() IS NOT A DECLARED SYMBOL
FIXED IN 7f08c23429dba76158bc3749e349010095ffdd18
REAL BUILD ATTEMPT 3: BUILD PASSED FAR ENOUGH TO RUN THE COMMAND
RUNTIME ATTEMPT 1: ASSERTED BECAUSE ScreenPercentageInterface WAS NULL
FIXED IN 6d16b7e29f6e512ab8afb1ee58755188d04eb8ce
RUNTIME ATTEMPT 2: OUTPUT_WRITTEN
STANDALONE FULL VIEW-FAMILY RENDERER SUBMISSION PASS
TRANSFORMED TARGET-SPACE GEOMETRY VISIBLE
FINAL OUTPUT EXPOSURE / DISPLAY DOMAIN NOT ACCEPTED
PRE-TONEMAP INTEGRATION STILL REQUIRED
NO CORE PORTAL FIDELITY SEAL CLAIMED
```

## Compile correction 1 — primitive renderer identity

The first real UE 5.8 compile found:

```text
InteriorPortalFullViewFamilySpike.cpp(82,53):
error C2039: 'ComponentId' is not a member of 'UPrimitiveComponent'
```

The spike had attempted:

```cpp
ViewInitOptions.HiddenPrimitives.Add(Primitive->ComponentId);
```

UE 5.8 exposes the renderer identity through the public
`UPrimitiveComponent::GetPrimitiveSceneId()` accessor. The code now uses:

```cpp
ViewInitOptions.HiddenPrimitives.Add(Primitive->GetPrimitiveSceneId());
```

## Compile correction 2 — renderer module access

The next real UE 5.8 compile reached the renderer submission point and found:

```text
InteriorPortalFullViewFamilySpike.cpp(320,3):
error C3861: 'GetRendererModule': identifier not found
```

The spike now acquires the Renderer module explicitly:

```cpp
IRendererModule& RendererModule =
    FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);
```

## Runtime correction 1 — required screen-percentage interface

The first command execution reached the renderer and then asserted:

```text
Assertion failed: InViewFamily->ScreenPercentageInterface
SceneRendering.cpp:3046
```

The standalone `FSceneViewFamilyContext` does not pass through the normal
`UGameViewportClient` setup, so the spike now installs the engine's legacy
screen-percentage driver explicitly:

```cpp
ViewFamily.SetScreenPercentageInterface(
    new FLegacyScreenPercentageDriver(ViewFamily, 1.0f));
```

`ShowFlags.SetScreenPercentage(false)` remains in place, so this satisfies the
renderer contract without enabling dynamic resolution.

## Runtime result 2 — full view-family output written

After the screen-percentage fix, `portal.RunFullViewFamilySpike` completed and
wrote all three requested artifacts:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

The JSON reports:

```text
status = OUTPUT_WRITTEN
sceneViewIsSceneCapture = false
rendererPath = IRendererModule::BeginRenderingViewFamily + standalone FSceneViewFamilyContext
renderTargetFormat = PF_FloatRGBA / RTF_RGBA16f
targetSize = 1742 x 874
endpointIndex = 0
```

The submitted transformed view was finite and produced coherent target-space
geometry. The output is visibly different from the earlier flat BaseColor CRP
proof: object faces and large surfaces contain smooth intensity/color variation
rather than only flat material-ID-like regions. Therefore the standalone full
`FSceneViewFamily` path has crossed the important feasibility boundary that the
`DepthAndBasePass` CRP could not cross: it can drive the normal scene renderer
from the portal virtual camera and produce non-empty processed scene output.

This result is recorded as:

```text
FULL VIEW-FAMILY RENDERER SUBMISSION = PASS
TRANSFORMED GEOMETRY / PROCESSED SCENE OUTPUT = PASS
FULL LIGHTING / LUMEN FEATURE MATRIX = NOT YET SEALED
EXPOSURE / DISPLAY PARITY = FAIL / UNACCEPTED FOR THIS OUTPUT
```

### Exposure-domain evidence

The exported PNG is heavily blown out. Inspection of the uploaded EXR confirms
that this is not merely an 8-bit PNG conversion artifact: the float EXR is
already bounded at approximately 1.0, with a large majority of pixels sitting
at or near the upper limit. The image therefore represents a final/display-like
output whose exposure / post-process state is not suitable for direct use as the
portal's pre-tonemap HDR source.

This is consistent with the spike's original limitation: EyeAdaptation was
deliberately disabled and the one-shot secondary ViewState has no established
player exposure history. The result must **not** be interpreted as a valid
exposure-matched production portal image.

### Architectural consequence

Do not fix the blown-out result with arbitrary brightness multipliers. The next
renderer task is to preserve the successful full transformed `FSceneViewFamily`
producer while changing the extraction boundary:

```text
full transformed secondary FSceneViewFamily
    -> normal deferred / lighting renderer
    -> capture secondary SceneColor at a pre-tonemap post-process boundary
    -> write that lit HDR SceneColor into the existing portal HDR target
    -> reuse existing main-view BeforeDOF analytic aperture composition
    -> let the player/main view remain the final exposure + tone-map authority
```

The existing `FInteriorPortalViewExtension` already subscribes to `BeforeDOF`
for main-view composition and intentionally skips `bAdditionalViewFamily`
secondary views. The next spike should add a separate, explicit secondary-family
capture path rather than removing that guard blindly or feeding the portal target
back into itself.

Still not proven by this result:

```text
exact direct-light / shadow acceptance across the full matrix
Lumen GI/reflection parity
translucency/fog/decal parity
main depth/stencil continuity
portal-bounded scissor
TAA/TSR + motion-vector history
exposure parity
recursion >= 2
production GPU cost
Core Portal Fidelity Seal
```

The installed UE 5.8 build remains the authority for exact runtime renderer
contracts. The next implementation should stay narrow: one transformed secondary
view, one portal, one frame, pre-tonemap SceneColor extraction only.
