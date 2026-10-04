# Receiving-view portal surface ownership

Updated: 2026-09-27. Status: IMPLEMENTED; BACKFACE/HDR CONTRACT CORRECTED; NORMAL-LIGHTING SEAM / CONTINUOUS VISUAL GATE OPEN.
Source baseline: `fd825aca848de01f120bf346badd0eab6f161bc8`.

## Problem and root cause

The user rejected the previous half-crossing seam candidate: the second portal
disappeared and oblique recursive openings acquired wall-colored gaps. Its depth-1,
703x246 fixed-camera evidence never established two-endpoint or recursive acceptance.

An ideal logical plane and a SceneDepth value cannot establish **surface identity**.
The actually rasterized support can lie microscopically before the ideal plane;
GPU projection, depth encoding and inverse reconstruction also introduce numerical
differences. Tightening the historical 2 cm depth tolerance treats that support as
foreground. Widening the tolerance instead loses real source-side traveller slices.
World-space float encoding before camera subtraction compounds the problem. These
are incompatible ownership contracts, not a portal size or physics-pose problem.

Targeted D3D12 controls identified the false foreground on the supporting wall.
Disabling depth propagation, hiding only the cosmetic Surface, disabling Nanite,
or comparing against a more precise ideal-plane equation did not remove it.
Temporarily hiding the support in disposable PIE did remove it; this is diagnostic
evidence only, never the production solution. Independent support depth AND identity
removed the same false foreground without removing ordinary foreground geometry.

## Rendering contract

Each endpoint request freezes its logical entry/exit frames, fallback primitive,
support stencil identity, crop, publication identity and rim parameters. For each
receiving view (player or recursive), the compositor replaces the support **only
inside the legal analytic aperture and only when that exact support owns the
receiving SceneDepth sample**. It replaces its own opaque fallback Surface only
when its child output is usable. It never globally hides a support or another
endpoint. Terminal fallback and independent endpoint publications remain intact.

The attachment also owns a **directed** opening. The parent/receiving camera must
be strictly in its positive logical half-space; transported child origins are
not facing tests. Planning, fallback exclusion and the shared color/rim/depth
shader reject back receivers, including stale front publications consumed after
crossing. The HDR-stage and lighting boundaries are documented in
[Directed views and scene-linear HDR](InteriorPortalSceneLinearHDRBoundary.md).

The receiver's stock CustomDepth/CustomStencil provides an independent raster of
eligible supports. Both depth and stencil must agree: the expected unique stencil
alone does not permit overwriting a gun/cube in front of that support. SceneDepth
and support depth must match within their depth encoding/float comparison bound.
Other samples use the logical plane's affine DeviceZ equation to retain actual
foreground. There is no centimeter clearance band. The same shader helper determines
ownership for color, rim and propagated depth.

CPU adds receiving PreViewTranslation to the logical origin in double before
float encoding. Analytic rays use UE's SVPositionToTranslatedWorld at the same
raster pixel as SceneDepth; actual receiving crop, viewport origin and temporal
jitter participate once. The affine depth equation derives from the float-encoded
receiving forward projection, inverted in double. Source/remote material slices
and native hardware exit clipping partition the same logical connection; the
cosmetic mesh offset no longer defines source-side foreground ownership.

## Attachment and backend lifecycle

`FInteriorPortalSupportDepthOwnership` is a rendering adapter, not physics authority.
It reserves the smallest available nonzero stencil in deterministic blue/orange
order. Existing unrelated identities are excluded; two endpoints on one support
share one lease. Existing enabled CustomDepth may be borrowed only with a unique,
nonzero stencil and full write mask; its state is unchanged. Ambiguous/exhausted
identities fail closed with `SUPPORT_DEPTH_IDENTITY_UNAVAILABLE`, preserving fallback.

A newly enabled support records its original stencil/write mask. Moving, clearing,
unlinking or losing a support releases its lease and restores its previous state
only if it still has the adapter's state. Active identities are checked against
current custom-depth users each update; a later alias revokes composition as well.
External mutations revoke composition and are never overwritten at teardown. Destroyed components are weakly referenced.

The backend enables depth-with-stencil mode (`r.CustomDepth=3`) and matching raster
jitter once at startup, retaining both across attachment changes. Stop restores
previous values if neither the value nor its console source priority changed.
Use explicit equal console priority, including constructor defaults, with the
normal UE console change notification: changing the value without it leaves UE's cached custom-depth mesh
commands in the wrong mode. UE re-registers scene components when this mode changes;
therefore this global cost belongs to backend startup/stop, never portal placement
or clear. No project configuration or map asset is saved.

The new stock depth pass costs support depth draws/storage in receiving views.
It adds no recursive ViewState, TSR history, color target or color copy. Ping-Pong
ownership, retirement, exposure authority and publication guards remain unchanged.
No performance, memory-budget or physics cutover acceptance follows from this fix.

## Validation and remaining acceptance

Required: prescribed UE 5.8 generation/build; focused FullFidelity, exposure,
main-view ownership, lifecycle and projection Automation; real D3D12 PIE in fresh
Ping-Pong 0/1 sessions. Test transported world precision, nonzero receiving viewport,
cropped projection, independent endpoint exclusions, support identity conflict/
exhaustion, shared support, replacement, external mutation and teardown restoration.

Prescribed generation and Development Editor build PASS. Initial final focused
D3D12 run: **19/20 PASS, zero test warnings**, UTC 2026.09.27-09.02.04. Only the
new support lifecycle test failed: SetWithCurrentPriority promotes constructor
settings to scalability priority in UE 5.8. Replace it with explicit equal-priority
normal Set, guard restoration by both value and priority, then rebuild PASS.
Affected lifecycle rerun: **1/1 PASS, zero test warnings/errors**, UTC
2026.09.27-09.05.51. Reuse the 19 unaffected successes; these are separate runs,
not an aggregate 20/20 final-head run. Their scope includes FullFidelity plus
MainViewOwnership, ColorSampleExposure, ViewExtensionLifecycle and ObliqueProjection.
The new gates cover translated origins, receiving raster projection and support
lease lifetime/conflict/exhaustion. No physics suite rerun is required for this
rendering interruption; existing PHY-4 evidence is unaffected.

Final managed-renderer D3D12 PIE (not the constant-stencil prototype): separate
fresh Ping-Pong **0 and 1**, **16 captures each**, 2112x1171 receiving viewport,
1920x1065 targets, primary fraction **0.67**, custom depth **3**, jitter **1**,
unique support IDs **1/2**; both sequences completed with no script error. Visual
inspection shows both direct endpoints, restored oblique recursive openings,
and retained gun/floor foreground. The cube's simulation was disabled in these
older fixtures, so its proxy/slicing was unregistered; withdraw the claimed
connected half-crossing acceptance. The
inspection-derived oblique camera is (1318.934185,816.002525,120.147999), yaw
109.494187, pitch -4.9402; right-wall exit (1790,750.182637,122.984421), yaw 180.
Requested depths 1-4 at that placement geometrically select at most two layers;
the separate facing fixture actually publishes all four levels. Static geometry
agrees across the two buffer modes; this is not pixel-identical temporal/exposure
acceptance or continuous-motion proof. All fixture changes are disposable PIE only.

Evidence (local/ignored):

- `Saved/Logs/PortalSupportOwnership{Final,Guard,Priority}ProjectFiles.log` and
  matching `Build.log`; final executable comes from the Priority build.
- `Saved/AutomationReports/PortalSupportOwnershipFinal/index.json` (19/20),
  `PortalSupportOwnershipPriority/index.json` (1/1), matching Automation logs.
- `Saved/AutomationReports/PortalReceivingPlaneP{0,1}OwnershipFinal/sequence.json`
  and PNGs; `Saved/Logs/PortalSupportOwnershipAcceptedP{0,1}.log`.

The initial managed-lease probe failed because it skipped UE's mode-change
notification; it is rejected evidence. The successful constant-stencil prototype
only established attribution. The later alias/console-priority guard does not
change shader geometry or the captured unique-ID steady-state mapping. Current
Priority-build D3D12 Ping-Pong 1 recheck completed five targeted scenes
(orange front, dual view, reported recursion, half-crossing slant, actual depth 4)
with no script error; the inspected openings remain intact. Its disabled-body
cube likewise does not establish actual split-traveller acceptance. Evidence:
`Saved/AutomationReports/PortalReceivingPlaneP1OwnershipPriority/sequence.json` and
PNGs, `Saved/Logs/PortalSupportOwnershipPriorityPIE.log` (end UTC 09.09.14).

Current backface/HDR correction: final prescribed generation/build PASS; focused
D3D12 Automation **22/22 PASS, zero test warnings/errors**, UTC 10.49.31,
`Saved/AutomationReports/PortalSceneLinearBoundaryFinal/index.json`. Fresh
Ping-Pong 0/1 corrected-HDR probes each complete 10 scenes at 2114x1173, with
simulation enabled and both live slices verified. Back masks 0, orange front 2,
dual mask 3, actual four-level layer mask 15. Albedo controls and raw HDR inputs
are near parity, but the normal-lighting cube seam remains OPEN. Exact root,
evidence, rejected fixtures and next lighting attribution are in the linked HDR
document. No static split-traveller color PASS is claimed.

USER ACTION REQUIRED for the final continuous held-cube/traversal gate in
`/Game/House/L_Interior_LivingKitchen`: half-cross the cube slowly, move/turn near
both portals, put a portal partly offscreen, repeat with recursion 1-4 and fresh
Ping-Pong 0/1 sessions. Both openings and recursive views must remain visible;
no cube strip, support speckles, stretched sampling, flicker or trails; the gun
must still occlude the opening. Provide a short recording of the failing angles
or confirm all listed observations. Static scripted captures supplement this gate.
