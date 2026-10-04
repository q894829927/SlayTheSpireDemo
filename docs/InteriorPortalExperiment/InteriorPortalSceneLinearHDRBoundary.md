# Directed portal views and the scene-linear HDR boundary

Updated: 2026-09-27. Source baseline: `fd825aca848de01f120bf346badd0eab6f161bc8`.
Backface/HDR contracts implemented; normal-lighting
traveller seam and continuous visual acceptance remain OPEN.

## Diagnosis and ownership

The user reported portals visible through the back of their supporting walls and
a diagonal brightness/color discontinuity through a genuinely half-crossing cube.
These are distinct from the earlier missing-coverage defect. Supporting primitive
identity alone identifies an attachment, not the direction of its opening.

For every player or recursive receiver, a native opening is usable only when
`dot(ReceivingViewOrigin - EntryLogicalOrigin, EntryLogicalNormal) > 0`.
Use the parent/receiving camera, never the transported child camera: the child
normally lies behind its exit plane. Reject nonfinite/coplanar cases, without a
centimeter deadband. Planning reports `ENTRY_BACKFACE`; actual receiving-view
surface exclusion and composition independently enforce the same condition so
an older front publication cannot expose a newly back-facing opening. The shared
color/rim/depth shader enforces the directed half-space as well. Back receivers
retain their normal supporting wall and fallback; no global component hiding.

An independent color defect was identified in UE 5.8's real postprocess call
chain. `EPostProcessingPass::Tonemap` means **after Tonemap**, not before it.
`SceneViewExtension.h` distinguishes `ReplacingTonemapper` from
`BL_SceneColorAfterTonemapping`; `PostProcessing.cpp` registers Tonemap delegates
in `GetAfterPassCallbacks`, invokes the default Tonemap pass, then calls
`AddAfterPass(EPass::Tonemap, SceneColor)`. The old auxiliary extraction therefore
published already transformed color as pre-exposed scene-linear HDR. The main
receiver processed it again. Numerical exposure rebasing tests alone did not
prove the texture's color domain.

The native auxiliary family owns a **ReplacingTonemapper** handoff. Its input is
resolved post-TSR, pre-exposed scene-linear color; extract that input and return it
unchanged instead of applying an auxiliary display transform. Eye adaptation is
computed earlier and independent per-layer ViewStates remain alive. Recursive
receivers compose the same HDR domain; the player view owns final display color
processing. The older single-view TSR diagnostic uses the same boundary, because
it shares this color packet and must remain a truthful comparison path.

`FColorSample` seals the exact submission's exposure only from this stage. Its
exposure is private/read-only after sealing; reject after-Tonemap, missing,
nonfinite/nonpositive and repeated seals. Consumption requires a sealed packet;
failure cannot borrow old exposure. Native publication still follows both queued
color/depth extractions and existing generation/identity checks. This does not
change Ping-Pong allocation, crop, per-layer history or resource retirement.

Additional families also bypass `UGameViewportClient::Draw`. Copying raw viewport
flags does not apply its quality and `ShowFlag.*` overrides. The shared internal
family policy applies `EngineShowFlagOverride(ESFIM_Game, VMI_Lit, ...)`, then
retains the producer's TSR/screen-percentage policy and receiver-owned DOF/motion
blur. It preserves the viewport's exposure quality policy. This fixes real
production flag divergence and makes unlit diagnostic controls valid.

## Corrected validation fixture

Earlier fixed-pose seam captures disabled simulation on the cube. Production
`MaintainPhysicsTravellerRegistration` unregisters nonsimulating bodies, destroys
their proxy and restores source slicing. Those captures **do not establish a
half-crossing source/proxy seam PASS**. Their unrelated opening/foreground
observations remain historical, but the traveller acceptance claim is withdrawn.

New disposable LivingKitchen PIE fixtures keep source simulation enabled, freeze
gravity/velocity and reduce world time dilation while resetting the pose. Runtime
bindings verify `SliceEnabled=1` on both the actual source body and live visible
mapped proxy. Nothing is saved to the map/material assets. This is static
rendering evidence, not actual Physics Handle motion or physics acceptance.

## Actual evidence

Prescribed bundled UE 5.8 project generation/build PASS. The initial HDR-stage
change passed focused D3D12 Automation **22/22, zero test warnings/errors**, UTC
2026.09.27-10.34.02. Scope: `Portals.FullFidelity`, `MainViewOwnership`,
`ColorSampleExposure`, `ViewExtensionLifecycle`, `ObliqueProjection`.
Includes actual per-view visibility/exposure ownership, directed receiving
origins including rotated/large-world/recursive cases, family quality overrides,
support identities and coordinate/lifetime regressions. Final private-accessor and
single-view diagnostic alignment: prescribed generation/build PASS; final focused
D3D12 **22/22 PASS, zero test warnings/errors**, UTC 2026.09.27-10.49.31.
These are separate runs, not a cumulative total.

Fresh D3D12 Ping-Pong **0 and 1** each completed **10 captures**, receiving viewport
2114x1173, targets 1920x1065, auxiliary primary fraction 0.67. Both back-facing
cases publish mask 0; orange front mask 2; dual-visible mask 3; facing fixture
actually publishes four levels, layer mask 15. Both half-crossing slices are live.
Inspected openings are intact across modes. Albedo controls remove the large
color step; a fine boundary line/TSR reconstruction difference remains visible.
Normal lit captures still show the reported brightness seam: overall color
acceptance is NOT PASS.

Successful filtered GPU dumps (`Base/Status.txt=ok`) inspect actual shader
parameters and raw PF_FloatRGBA input resources before composition. In the flat
cyan patch, albedo controls have exposure scale 1 in both buffer modes:

| Input / stage | Linear RGB |
|---|---|
| Main proxy, before composition | (0.0979614, 0.3679199, 0.5678711) |
| Old auxiliary after-Tonemap input, before this fix | (0.0632324, 0.2775879, 0.4279785) |
| Corrected auxiliary scene-linear input, Ping-Pong 0 and 1 | (0.0976563, 0.3671875, 0.5625000) |

The small corrected difference is retained evidence, not asserted pixel identity.
Before the fix, separate BasePass raw captures already showed equal source/proxy
albedo. GPU coordinates derive from captured receiving viewport and sampling
parameters, not texture extent alone; the auxiliary primary viewport is normalized
by UE. Do not infer an internal primary origin from the requested crop rectangle.

Fresh corrected-HDR normal/GI controls still show a large difference **before
main Tonemap**. At representative flat-patch interior pixels, normalized source
RGB is (0.0085072, 0.0381674, 0.0625394), main proxy RGB is
(0.0490417, 0.2309570, 0.4228516). Disabling Lumen diffuse indirect only for
diagnosis reduces it to source (0.0530949, 0.1206234, 0.2144416), proxy
(0.0528870, 0.1322021, 0.2469482). This attributes a dominant remaining component
to indirect lighting, not a missing face or a display-domain exposure correction.
It does not prove a single precise cause inside Lumen or pixel-perfect GI-off parity.

A same-world-point attribution control also completed. Hide both supports and the
exit cosmetic Surface only in disposable PIE, hold PreExposureOverride=1 for both
views, retain real source/proxy slicing and normal lighting, then place the primary
camera at the exact transported auxiliary position/orientation. The cyan source
surface at screen (1000,600) has auxiliary HDR (0.578125,3.250000,7.375000) and
primary pre-tonemap HDR (0.5703125,3.187500,7.375000). Actual camera is
(1831.403417,1288.158322,120.150002), pitch -17.489751, yaw -112.562885. Main
telemetry confirms PreExposure=1 and EyeAdaptation=1; auxiliary rebase scale=1.
Primary HDR is decoded from its captured PF_FloatRGB/R11G11B10 texture with its
actual extent/viewport. This rules out a large inherent primary/additional
color multiplier **in that controlled scene**, not all Lumen visibility/history
differences in the unmodified lit scene. Wall hiding and exposure override are
diagnosis only; all temporary state is discarded/restored at process shutdown.

Local/ignored evidence:

- `Saved/Logs/PortalSceneLinearHDR{ProjectFiles,Build,Automation}.log`;
  `Saved/AutomationReports/PortalSceneLinearHDR/index.json` (22/22).
- `Saved/AutomationReports/PortalReceivingPlaneP{0,1}SceneLinearHDR/sequence.json`,
  PNGs and `albedo_control_gpu/color_samples.json`;
  `Saved/Logs/PortalSceneLinearHDRP{0,1}Probe.log`.
- `Saved/AutomationReports/PortalReceivingPlaneP1SceneLinearHDRLighting/`,
  normal/GI-off PNGs and raw GPU color samples;
  `Saved/Logs/PortalSceneLinearHDRLightingP1Probe.log`.
- Earlier `PortalReceivingPlaneP1BasePassGPU` establishes albedo attribution;
  older after-Tonemap GPU data is diagnosis, not current HDR acceptance.
- `Saved/Logs/PortalSceneLinearBoundaryFinal{ProjectFiles,Build,Automation}.log`;
  `Saved/AutomationReports/PortalSceneLinearBoundaryFinal/index.json` (22/22).
- Final executable compact PIE smoke completes three captures: registered
  albedo half-crossing, actual native four-level recursion and exclusively enabled
  single-view TSR diagnostic. Native geometry remains intact; diagnostic records
  90 submitted frames, completed color/depth extraction at frame 290, AA method 4,
  jitter and valid exposure/history at the corrected hook. This establishes its
  HDR/exposure handoff, not legacy geometry/visual parity acceptance.
  `Saved/AutomationReports/PortalReceivingPlaneP1SceneLinearBoundaryFinalSmoke/`,
  `Saved/Logs/PortalSceneLinearBoundaryFinalSmoke.log` (end UTC 10.51.32).
- `Saved/AutomationReports/PortalReceivingPlaneP1SameWorldLighting/`:
  two same-point captures, completed=true, recorded source/proxy bindings and
  primary telemetry, successful filtered auxiliary-composition/primary-Tonemap
  raw GPU inputs; `Saved/Logs/PortalSameWorldLightingProbe.log` (end UTC 10.59.34).

The uncontrolled all-resources GPU dump is rejected evidence. Automatic approval
rejected cleanup with only `blocked by policy`; approximately 30.09 GB remains in
`Saved/GPUDumps/SlayTheSpireDemo-WindowsEditor-2026.09.27-18.13.32`.
Do not silently bypass that rejection or treat it as a tracked project artifact.

## Remaining lighting architecture and next action

The current compositor connects camera rays, material slices, color domains and
depth. Stock Lumen still evaluates the original body and mapped proxy in their
different world-space surroundings; analytic color/depth composition does not
connect shadow, distance-field, surface-cache or irradiance visibility through
the aperture. Correct render coordinates do not establish light-transport parity.

The controlled same-world-point check above does not reproduce the large split.
Next define the lighting contract for the actual source/mapped traveller, including
which local lights/indirect visibility reach each half and how those rays cross
the connection. A bounded portal-aware lighting design must distinguish local
world lighting from transported light, own the logical aperture/ray segments,
source/proxy shadow/DF/surface-cache representation and history lifetime, and
declare supported-light/recursion budgets before implementation. Keep an
unmodified normal-lighting reference: only the controlled scene has primary/
additional near parity, so further identified Lumen view defects remain in scope.
The user explicitly requested cross-portal lighting work on 2026-09-27.
[Lighting-path architecture and installed-engine feasibility](InteriorPortalLightTransport.md)
now owns that independent initiative, including an offline reference and a
project-only native TLAS probe. General light transport remains outside the
original ImplementationPlan section 2.9 acceptance; do not silently declare
it implemented or use a visibility probe to accept the current reported seam.

No brightness gain, unlit material substitution, global GI disablement, widened
slices, suppressed proxy, shared unrelated ViewState or map-lighting edit is a
production solution. The current defect stays OPEN, and PHY-4 continuation/Core/P8
acceptance is not advanced by these rendering results.

USER ACTION REQUIRED for continuous visual acceptance in
`/Game/House/L_Interior_LivingKitchen`: view both walls from front/back; insert and
withdraw the held cube, turn rapidly and move openings offscreen, then traverse;
repeat fresh Ping-Pong 0/1 sessions and depths 1-4. Back walls must remain opaque,
front/recursive openings and gun occlusion intact. Report/record remaining thin
seams or temporal artifacts. Normal-lighting color discontinuity is already known
OPEN and must not be marked accepted merely because the geometry check passes.
