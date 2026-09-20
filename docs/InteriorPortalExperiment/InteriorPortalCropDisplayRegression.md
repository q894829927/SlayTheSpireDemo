# Interior Portal — cropped display regression

Date: **2026-09-20**. Branch: `portal/full-fidelity-p1`.
Implementation base HEAD: `df09691ff277c055621a18602c8d93f1086f7f78`;
the repair is an uncommitted working-tree change on that base.

## Symptom and confirmed cause

The remote image occupied a thin vertical aperture while the blue/orange
fallback spiral remained visible around it. Its apparent width changed as the
player approached. This is different from the normal perspective growth of the
physical portal.

On the user's running PIE camera, with Ping-Pong enabled, setting
`portal.BoundedMainPassScissor 0` removed the slit and setting it back to `1`
reproduced it. Evidence: `Saved/PortalCrop_BoundedOff.png` and
`Saved/PortalCrop_BoundedOn.png`; pose: `Saved/PortalCropScene.json`.

`ScreenPassVS` supplies raster-pass-local clip coordinates in
`UVAndScreenPos.zw`. Bounded composition uses a viewport restricted to the
portal's projected rectangle. Feeding these coordinates into the complete
receiving view's `ScreenToTranslatedWorld` reconstructs the wrong rays and
shrinks/displaces the analytic aperture. Both color composition and the
transported-depth candidate shared this error. Ping-Pong enables bounded
composition at startup, exposing the latent bounded-pass problem.

This reproduced regression reopens the affected coordinate contract of the
historical bounded-main-pass acceptance. It does not reopen unrelated exposure,
native lifecycle or traversal contracts.

## Repair contract

- The compositor supplies an absolute-pixel-to-camera-NDC transform derived
  from the **complete receiving SceneColor view rectangle**, including its
  nonzero origin. Color aperture and depth candidate use this same transform.
  Raster/scissor rectangles affect the work performed, not camera rays.
- Cropped secondary projections and the full-coordinate-domain Ping-Pong
  color/depth buffers remain in use. Parent-view texture sampling and per-level
  ViewState/TSR/Lumen ownership remain unchanged.
- Secondary depth extraction reads the primary view rectangle from
  `View.ViewRectMinAndSize` in the extraction shader. It no longer scales the
  destination crop by the pooled depth texture's full allocation extent.
  Post-TSR color and primary-resolution depth map their respective actual view
  rectangles into the same destination rectangle; depth uses point loads.
- A submission does not publish a new color sample if its depth extraction
  could not be scheduled. Existing lifetime/publication identity checks remain.
- Dump schema advances to `PortalFullFidelityPingPongViewport.Prototype.v2`.
  The inferred `depthSourceWidth/Height` fields are replaced by
  `depthSourceRectAuthority="View.ViewRectMinAndSize"`; these are shader-owned
  coordinates, not purported CPU measurements. Optional extraction diagnostics
  log actual color source and destination rectangles.

The optimization remains available, but `portal.FullFidelityPingPong` defaults
to `0` pending the final continuous-motion manual gate, as requested. Set it to
`1` before a fresh PIE to validate the repaired optimization. This only changes
the output policy; TSR/Lumen quality is retained.

No gameplay API, map, portal size, FOV, quality setting or engine/plugin/build
configuration was changed. The pre-existing map modification is retained;
SHA256: `1F8CCDC4B8A5D82B8DB469D8F6F4D290F961FAABE5E17708B89706103ED26E5B`.

## Automated gates

- Bundled UE 5.8 project generation: **PASS**,
  `Saved/Logs/PortalCropProjectFiles.log`.
- Development Editor Win64 build: **PASS**, `Saved/Logs/PortalCropBuild.log`.
- Final regeneration/build after changing only the temporary default to `0`:
  **PASS**, `Saved/Logs/PortalCropFinalProjectFiles.log` and
  `Saved/Logs/PortalCropFinalBuild.log`. The passing CPU tests below are reused:
  the default-only change does not alter their tested coordinate/lifetime contracts.
- Focused Automation, **9 succeeded, 0 failed, 0 not run**:
  `Saved/AutomationReports/PortalCropRepair/index.json` and
  `Saved/Logs/PortalCropAutomation.log`.
  Selected prefixes: `SlayTheSpireDemo.Interior.Portals.FullFidelity`,
  `SlayTheSpireDemo.Interior.Portals.ColorSampleExposure`, and
  `SlayTheSpireDemo.Interior.Portals.MainViewOwnership`.
  The command also specified `SlayTheSpireDemo.Interior.Portals.RecursionLifetime`,
  which matches no test; the real `FullFidelity.P1A2.LifetimeModel` is included
  under the FullFidelity prefix.
- New `FullFidelity.ProjectedBounds.ReceivingViewCoordinates` coverage checks
  a nonzero viewport origin, off-center aperture, eleven moving crop sizes,
  and recursive crop composition preserving the original pixel/ray mapping.

Automation used `-NullRHI`; it proves CPU contracts, not shader execution.
The new extraction shader initially failed D3D12 PSO creation because its input
signature omitted the ScreenPass vertex interpolant. This was corrected to the
matching `noperspective TEXCOORD0` / `SV_POSITION` signature. The subsequent
real D3D12 PIE sessions executed the shader successfully. The initial failed
GPU run is preserved in `Saved/Logs/PortalCropPIE.log`; it is not passing evidence.

## PIE evidence and remaining gate

Fresh PIE sessions were used for Ping-Pong 0 and 1 because the switch is latched
at renderer startup. Corrected captures are
`Saved/PortalCrop_After_P{0,1}_{Far,Mid,Near}.png`. Pawn X=1317.5927,
Y=494/644/769, Z=72.15; control rotation pitch=-4.9402, yaw=92.738, roll=0.
Both paths fill the aperture without the vertical slit in the inspected
captures. The original near-pose optimized output also matches the
pre-repair bounded-pass-off geometry. These are static visual observations,
not a continuous-motion acceptance claim.

Discard `PortalCrop_Before_P{0,1}_{Far,Mid,Near}.png` as A/B evidence: the first
capture helper used positional Python Rotator arguments in the wrong order.
The corrected helper uses named arguments and checks the actual camera yaw.
The original `PortalCrop_BoundedOff/On.png` pair did not use that helper and
remains valid.

The full-view (Ping-Pong=0) extended matrix captured depths 1/2/3, then the
process exited with D3D12 **out of video memory** when advancing to depth 4.
That run had the D3D debug layer enabled. Evidence:
`Saved/Logs/PortalCropPIE2.log`, `Saved/PortalCrop_P0_Depth3BeforeOOM.json`.
This is a failed resource gate, not evidence of a crop-coordinate failure or a
successful full depth-4 comparison. Resource-budget work remains outside this
repair's scope.

Optimized extended matrix: **13 scenarios captured**, in a fresh process
without the D3D debug layer. Evidence: `Saved/PortalCropMatrix_P1.json`,
`Saved/PortalCrop_Matrix_P1_*.png`, `Saved/Logs/PortalCropPIEFinal.log`.
Scenarios include requested depths 1/2/3/4, left/right oblique views, partial
offscreen aperture, offscreen/return, aperture mask, foreground depth, remote
depth and depth-write diagnostics. Inspected aperture/depth diagnostic images
fill the intended aperture; remote-depth cyan and depth-write cyan/foreground
green replace the erroneous slit. This session used a 2556x566 viewport and
1920x425 output targets; do not compare its pixels directly to the earlier
2556x1093 captures.

Requested depth is not actual submitted depth: most original-map poses had
one visible layer per endpoint; right/partial poses had two layers on one
endpoint. A separate **PIE-only** facing-pair fixture moved orange to
(1300,450,105), yaw=90, with the player at (1300,700,72.15), yaw=90.
`Saved/PortalCropNested.json` proves all four blue layers submitted and
published (`0b1111`); `Saved/PortalCrop_Nested4.png` shows the nested apertures.
The fixture restored the original runtime transforms/depth and did not save
the map. The terminal spiral at the recursion limit is expected, unlike the
incorrect spiral around the first aperture in the original defect.

**USER ACTION REQUIRED — final continuous-motion visual acceptance:** on
`/Game/House/L_Interior_LivingKitchen`, set `portal.FullFidelityPingPong 1` in
the editor console before starting a fresh PIE;
approach/retreat, view both sides obliquely, move a portal partly offscreen,
rapidly turn away/back, and walk through/back with the gun in front of the
aperture. Expect continuous remote parallax, no slit/stretch/spiral flash and
correct foreground occlusion. Record observed result and a short clip for any
failure. Depth-4 full-view comparison remains unaccepted after the OOM.
