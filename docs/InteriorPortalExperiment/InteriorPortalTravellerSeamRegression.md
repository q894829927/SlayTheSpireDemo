# Half-crossing traveller visual seam

Updated: 2026-09-27. Status: **SUPERSEDED AS A COMPLETE FIX; RECURSIVE/TWO-ENDPOINT VISUAL GATES REOPENED**.
Source baseline: `fd825aca848de01f120bf346badd0eab6f161bc8`; this narrow fix was superseded.

The user subsequently reported a missing second portal and broken recursive
openings. The depth-1 fixed-pose evidence below is historical and cannot accept
this candidate. The complete receiving-surface ownership contract and current
validation now live in [Receiving-plane ownership](InteriorPortalReceivingPlaneOwnership.md).
The backface/color continuation lives in
[Scene-linear HDR boundary](InteriorPortalSceneLinearHDRBoundary.md). Backface
and incorrect after-Tonemap extraction are corrected; the actual lit traveller
seam remains OPEN and its dominant remaining component is indirect lighting.
Logical slicing/rim changes below remain part of that implementation, but strict
ideal-plane depth alone is insufficient to identify the supporting geometry.

## Report and diagnosis boundary

The supplied LivingKitchen screenshot shows a wall-colored diagonal strip through
the held test cube. The user confirmed it occurs **only while half crossing**;
the fully transferred cube is intact. Do not classify this as PHY-4 acceptance,
or assume physics pose corruption from this screenshot.

Code inspection identified an incompatible partition: source/remote material
slices retain their respective positive logical half-spaces, while the native
remote clip rejects the first 0.5 cm of the exit half-space. Main foreground
ownership uses the opaque cosmetic Surface, 0.6 cm before the logical plane,
and a further 2 cm line-of-sight tolerance. The opaque Surface can itself hide
the source slice before composition; merely lowering the console tolerance
cannot recover that lost color. These are concrete coverage defects. Exact
attribution of the reported strip remains subject to the focused visual gate.

Read-only MCP inspection found no active PIE session and
`portal.DepthOcclusionEpsilonCm=2`, `portal.CompositionDebugMode=0`. These are
editor-global observations, not a reconstruction of the screenshot's runtime.

## Ownership contract and migration

For endpoint-owned FullFidelity, the compositor owns the opening **and rim** on
the logical plane. Its immutable request carries the surface primitive ID, rim
color/time and explicit logical composition policy. Main-view BeginRenderViewFamily excludes
only that endpoint's opaque fallback Surface. It does not change component
visibility, collision, map assets or gameplay interfaces. Clear/disable restores
the normal per-view fallback. In recursive parent views the opaque entry Surface
is excluded only when a current-frame child can be consumed; terminal layers
retain their existing fallback. Publication/history/retirement guards are intact.

The existing procedural rim moves into the compositor in the receiving view's
pre-exposure domain and uses the same foreground ownership as the opening. Source
material slicing, native exit clip and main foreground depth all derive from the
logical plane. Hardware clipping keeps distance==0, so a strict-positive exit
half-space uses only a float-roundoff bound based on translated camera/plane
distance (minimum 0.0001 cm), excluding the coplanar host wall. It no longer uses
the authored 0.5 cm exclusion band. Normal source-side geometry immediately
before the logical plane is preserved;
color and propagated depth share `InteriorPortalDepthOwnership.ush`. Only a
small relative floating-point comparison allowance remains, not a centimeter
exclusion band. Legacy diagnostic/SceneCapture requests retain their prior
cosmetic-surface depth policy and console tolerance. Cosmetic bias and legacy
clip settings remain authored data, with no effect on this native partition.

This supersedes the cosmetic foreground reference for endpoint-owned production
in [the earlier grazing fix](InteriorPortalGrazingForegroundDepthRegression.md).
That fix correctly avoided treating the opaque Surface as foreground, but did
not establish continuous ownership of a sliced traveller next to it. Its legacy
diagnostic behavior remains. Do not move slice planes or physics poses to conceal
the seam, disable foreground occlusion, duplicate physics bodies or widen the
slices to overlap.

This is a specific rendering defect correction. PHY-4 production cutover/Core
and broader P8 visual generalization remain OPEN and keep their dependencies.

## Automated gates

- Prescribed bundled UE 5.8 project generation, then Development Editor build.
- `Portals.FullFidelity.TravellerVisualPartition`: transformed endpoint pairs,
  depths 1–4 and samples +/-0.01, 0.1 and 1 cm next to the seam; complementary
  source/remote material half-spaces and actual request hardware clip agree.
- `Portals.FullFidelity.CompositedSurfaceOwnership`: actual view-extension hooks,
  nonzero viewport origin, unrelated exclusions retained, additional-view
  isolation, clear and disable restore fallback visibility. Model UE's real
  ordering: SetupView runs before the viewport assigns its main-family flag;
  exclusion occurs in BeginRenderViewFamily before renderer view copies.
- Existing affected FullFidelity geometry/production and exposure/view-extension
  lifecycle regression tests. GPU startup must compile changed global shaders.

Prescribed generation/build PASS (`Saved/Logs/PortalTravellerVisualProjectFiles.log`
and `PortalTravellerVisualBuild.log`). Initial compile found a test-only shared
reference Reset misuse; change the fixture to TSharedPtr and rebuild PASS. The
earlier Live Coding rejection was resolved by closing the editor. The user also
authorized closing it autonomously for future builds, preserving desired edits.

Initial D3D12 Automation passed 17 tests at 04.13.24 UTC and compiled both changed
global shaders, but a fixed-pose PIE probe exposed an insufficient fixture:
SetupView saw no main-family identity and left the opaque Surface visible
(spiral retained, foreground diagnostic red across the opening). Move exclusion
to BeginRenderViewFamily after UE assigns identity, before renderer view copies;
strengthen the lifecycle-order fixture. An intermediate affected 3-test rerun
passed. Normalize the moved emissive rim against the receiving exposure domain,
and bound strict exit-plane ties by float precision instead of retaining the
coplanar support or using the old centimeter bias. The rim shader recompiled in
the D3D12 PIE run. These corrections invalidate the initial visual evidence.

Final build PASS and final D3D12 focused Automation **17/17 PASS, zero test
warnings/errors**, 2026.09.27-04.30.49 UTC: `Portals.FullFidelity`, `MainViewOwnership`,
`ColorSampleExposure`, `ViewExtensionLifecycle`, `ObliqueProjection`. Both changed
global shaders had compiled during this work; final run uses the current shader
cache. Evidence: `Saved/AutomationReports/PortalTravellerVisualComplete/index.json`
and `Saved/Logs/PortalTravellerVisualCompleteAutomation.log`. Runs are separate,
not a cumulative total. No transient visual PASS is inferred.

## Scripted fixed-pose PIE evidence

Final D3D12 PIE probes used the current compiled fix in LivingKitchen, with
`portal.FullFidelityPingPong 0` and `1` set before separate fresh PIE sessions.
Both completed without script errors. Recursion depth was 1, viewport 703x246;
the existing registered 40 cm cube was fixed in disposable PIE state with its
simulation disabled to isolate rendering. No map asset was saved.

Each mode captured five cases: head-on half crossing, oblique half crossing,
cube center 10 cm on either side of the seam, and foreground diagnostic mode 4.
Those PNGs show an intact ordinary cube and visible blue rim, but the claimed
half-crossing acceptance is **WITHDRAWN**: disabling simulation makes production
registration destroy the proxy and restore source slicing. These fixtures never
proved two live complementary slices. Diagnostic captures distinguish ordinary
foreground from the opening;
fine boundary noise remains visible in the oblique diagnostic, so this evidence
does not establish full-resolution grazing or temporal stability acceptance.

Evidence: `Saved/AutomationReports/PortalTravellerVisualPIECompleteP0/` and
`PortalTravellerVisualPIECompleteP1/`, each containing the five PNGs and
`sequence.json` (completed=true, error=null, actual mode and poses recorded).
Logs: `Saved/Logs/PortalTravellerVisualPIECompleteP{0,1}.log`; sessions ended at
04.31.58 and 04.33.35 UTC respectively on 2026-09-27.

This is an ordinary fixed-pose foreground check, not actual half-crossing,
held-body motion, depth-4
recursion, offscreen/rapid-turn stability or complete gun-occlusion acceptance.
The focused manual gate below remains OPEN.

The corrected fixture retains source simulation and verifies both source/proxy
`SliceEnabled=1`. Current static coverage is intact, and true HDR/albedo inputs
are near parity, but normal Lumen lighting retains a diagonal brightness seam.
Do not accept this defect based on unlit controls or numeric exposure alone.

## USER ACTION REQUIRED — focused visual gate

After the build, open `/Game/House/L_Interior_LivingKitchen`, start a fresh native
FullFidelity PIE session and use the existing test cube and linked portals.

1. Hold the cube halfway through each endpoint. Slowly insert/withdraw it; inspect
   head-on, from both sides and while looking down at the reported pose. The cube
   must remain continuous with no wall-colored strip, doubled edge or lost face.
2. Repeat at recursion depth 1 and 4 and with Ping-Pong 0/1 in separate fresh PIE
   sessions. The same geometry must be visible. Avoid a VRAM-stressed run as the
   sole evidence for this gate.
3. Obliquely view both portals, move an opening partly offscreen and turn quickly
   away/back. No new spiral flash, stale image or disappearing opening/rim.
4. Overlap the gun with the opening/rim before the plane; it must retain correct
   foreground occlusion. Confirm the unlinked appearance and stop/restart fallback.

Record the result and a short clip if any seam/flicker remains. Shader compilation
and Automation do not establish this transient visual gate. Do not close the
reported defect before this evidence exists.

User map SHA256 remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.
