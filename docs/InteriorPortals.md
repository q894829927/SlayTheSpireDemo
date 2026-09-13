# Interior portal mechanism

Scope: `/Game/House/L_Interior_LivingKitchen`, including its existing corridor extension. This independent exploration feature does not advance battle phases.

Dedicated current implementation authority: [`InteriorPortalFullFidelityImplementationPlan.md`](InteriorPortalFullFidelityImplementationPlan.md).

## Intended behavior

One map-owned `InteriorPortalSystem` references blue/orange endpoints, an explicit list of portalable surfaces and the current physics traveller configuration. G equips/holsters portal input; LMB/RMB place blue/orange, R clears, E retains door/light interaction. The initial pair is accessible near the current corridor PlayerStart. Existing map and character work is preserved.

Rigid frames use +X outward, +Y horizontal, +Z vertical. Crossing transforms position, viewing orientation, linear velocity and rigid-body angular velocity through a 180-degree local Z rotation between endpoints. Swept front-to-back plane crossings avoid overlap-trigger teleports and speed-dependent missed planes. Closed/unpaired portals do not traverse. Placement requires a complete supported rim, an allowed planar surface, free opening and non-overlap. Moving or clearing a portal during traversal is rejected.

The player movement component constrains each submove to a continuous legal capsule interval before bypassing its supporting primitive. Explicit crossing/clearance state prevents immediate reverse transfer until the capsule clears. Physics travellers still use the prototype free Chaos constraints with pairwise support collision disabled. Teardown restores movement ignores and removes temporary constraints. Shared wall collision is never disabled globally.

Scene captures run after the local player camera update. Full viewport aspect/projection is retained. Native SceneCapture clipping is the current production candidate, with guarded oblique projection retained for comparison/fallback. The logical aperture and cosmetic surface bias are separate. Finite deepest-first captures use HDR targets. An emissive oval material shows the capture in screen coordinates and animated blue/orange rim detail.

Implementation reference: [Froyok, Creating Seamless Portals](https://www.froyok.fr/blog/2019-03-creating-seamless-portals-in-unreal-engine-4/). The implementation uses original project code and generated procedural materials, without Valve game assets or third-party plugins.

## Current implementation state — 2026-09-13

The portal feature is now materially implemented on `main`: C++ endpoint/system/math/camera code, generated portal materials, map setup, recursive SceneCapture rendering, player traversal, rigid-body transfer, Physics Handle support, visual slice proxies and focused portal Automation are present.

The feature is **not yet full-fidelity accepted**. Native clipping mitigated the reproduced black-wall/diagonal-wedge issue in the observed normal views; its full near/grazing matrix remains open. Capture/player exposure parity and manual acceptance of the player/held-item partial visuals and portal-aware flashlight illumination remain open in [the observed issue log](InteriorPortalObservedIssues.md).

The current delivery adds player submove aperture constraints, explicit crossing/clearance states, same-frame transfer protection, quaternion camera/input ownership, early transfer before destination floor queries, a player presentation component that slices tagged first-person meshes and maps the handheld flashlight into the paired aperture, and a bounded PortalQuery layer for line traces and sphere sweeps. PortalQuery analytically arbitrates a support-wall hit inside the legal aperture, maps the remaining segment through the pair and is reused by Physics Handle and player interaction targeting. Generated slice/light-function assets are bound to the map-owned system. Detailed scope, the reproduced large-timestep floor defect, actual-map reversal evidence and presentation runtime evidence are recorded in [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md). It preserves the user's existing flashlight-clearance and material/map edits.

The latest MCP recovery restored `M_InteriorPortal` after an experimental exposure
graph produced a black surface. The setup script now refuses to clear the graph
when its required UE expression is unavailable. A fresh PIE capture shows the
portal surface again; the P2 direct-vs-portal exposure comparison remains open
until the planned visual matrix is completed.

The follow-up wall-stuck fix keeps outward retreat and movement toward the aperture available after a small capsule footprint correction, while deeper wall entry remains blocked. Focused portal Automation and actual-map slow-entry/pause/rim-retreat replay each pass 6/6; the same traversal document records evidence and remaining visual acceptance.

Further required work now centers on the remaining acceptance gates: high-speed
Chaos aperture-local collision, the complete geometry/shape-fit matrix for the
generic traveller registry, dual-space remote collision/contact bridging for
partially crossed rigid bodies, PortalQuery segmented diagnostics and
exploration weapon/projectile coverage, lifecycle/performance profiling,
exposure parity and the final manual PIE matrix. The registry and conservative
sphere/capsule/box fit implementation are present; their broader acceptance
matrix is not yet sealed.

## Acceptance

AUTOMATED GATES: UE 5.8 project generation and Development Editor build; focused `SlayTheSpireDemo.Interior.Portals` tests; saved-map pair/material/surface load checks; traversal, velocity, placement and cleanup assertions; additional portal-query/traveller tests as those stages land.

MANUAL PIE GATES: moving-view parallax, both travel directions, near-plane transition, grazing-angle stability, facing-portal recursion, wall/floor momentum transitions, rigid-body angular momentum, held-object passage, partial-crossing visuals and remote contact behavior; verify existing E/Q/F/M behavior.

Single local player is the current product scope. Finite recursion and SceneCapture-specific temporal/lighting behavior must be assessed explicitly; C++ tests do not prove visual parity, Chaos contact quality or packaged behavior.

The final detailed Definition of Done and staged execution order live in `docs/InteriorPortalFullFidelityImplementationPlan.md`.
