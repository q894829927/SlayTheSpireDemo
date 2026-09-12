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

The feature is **not yet full-fidelity accepted**. Native clipping mitigated the reproduced black-wall/diagonal-wedge issue in the observed normal views; its full near/grazing matrix remains open. Capture/player exposure parity, player/held-item partial visuals and portal-aware flashlight illumination remain blocking items in [the observed issue log](InteriorPortalObservedIssues.md).

The current delivery adds player submove aperture constraints, explicit crossing/clearance states, same-frame transfer protection, quaternion camera/input ownership, and early transfer before destination floor queries. Detailed scope, the reproduced large-timestep floor defect and actual-map reversal evidence are recorded in [InteriorPortalPlayerTraversal.md](InteriorPortalPlayerTraversal.md). It preserves the user's existing flashlight-clearance and material/map edits.

Further required work includes Chaos aperture-local collision hardening, generic traveller registration and geometry-aware conservative body fit, production slicing across material slots, dual-space remote collision/contact bridging for partially crossed rigid bodies, portal-aware traces/sweeps, lifecycle hardening, recursion/performance profiling and the final manual PIE matrix.

## Acceptance

AUTOMATED GATES: UE 5.8 project generation and Development Editor build; focused `SlayTheSpireDemo.Interior.Portals` tests; saved-map pair/material/surface load checks; traversal, velocity, placement and cleanup assertions; additional portal-query/traveller tests as those stages land.

MANUAL PIE GATES: moving-view parallax, both travel directions, near-plane transition, grazing-angle stability, facing-portal recursion, wall/floor momentum transitions, rigid-body angular momentum, held-object passage, partial-crossing visuals and remote contact behavior; verify existing E/Q/F/M behavior.

Single local player is the current product scope. Finite recursion and SceneCapture-specific temporal/lighting behavior must be assessed explicitly; C++ tests do not prove visual parity, Chaos contact quality or packaged behavior.

The final detailed Definition of Done and staged execution order live in `docs/InteriorPortalFullFidelityImplementationPlan.md`.
