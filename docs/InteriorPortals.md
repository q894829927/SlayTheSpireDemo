# Interior portal mechanism

Scope: `/Game/House/L_Interior_LivingKitchen`, including its existing corridor extension. This independent exploration feature does not advance battle phases.

## Intended behavior

One map-owned `InteriorPortalSystem` references blue/orange endpoints, an explicit list of portalable surfaces and an ordered list of physics travellers. G equips/holsters portal input; LMB/RMB place blue/orange, R clears, E retains door/light interaction. The initial pair is accessible near the current corridor PlayerStart. Existing map and character work is preserved.

Rigid frames use +X outward, +Y horizontal, +Z vertical. Crossing transforms position, viewing orientation, linear velocity and rigid-body angular velocity through a 180-degree local Z rotation between endpoints. Swept front-to-back plane crossings avoid overlap-trigger teleports and speed-dependent missed planes. Closed/unpaired portals do not traverse. Placement requires a complete supported rim, an allowed planar surface, free opening and non-overlap. Moving or clearing a portal during traversal is rejected.

Pre-movement gates ignore only the character's supporting primitive. Physics travellers use entirely free Chaos constraints with pairwise collision disabled against that primitive. Teardown restores movement ignores and removes temporary constraints. Shared wall collision is never disabled globally.

Scene captures run after the local player camera update. Full viewport aspect/projection is retained; reversed-Z oblique projection clips geometry behind the exit without changing project shader settings. Recursive captures render deepest-first into separate HDR targets (default depth 3; finite limit). An emissive oval material shows the capture in screen coordinates and animated blue/orange rim detail. This render-target approach is not an assertion of pixel identity with Valve's renderer.

Implementation reference: [Froyok, Creating Seamless Portals](https://www.froyok.fr/blog/2019-03-creating-seamless-portals-in-unreal-engine-4/). The implementation uses original project code and generated procedural materials, without Valve game assets or third-party plugins.

## Acceptance and limitations

AUTOMATED GATES: UE 5.8 project generation and Development Editor build; focused `SlayTheSpireDemo.Interior.Portals` tests; saved-map pair/material/surface load checks; actual-map traversal, velocity, placement and cleanup assertions.

MANUAL PIE GATES: inspect moving-view parallax, both travel directions, near-plane transition, facing-portal recursion, wall/floor momentum transitions and physics-body passage; verify existing E/Q/F/M behavior. Single local player only. Finite recursion, temporal lighting differences and body slicing must be assessed explicitly; C++ tests do not prove visual parity or packaged behavior.

2026-09-12 interrupted implementation: project generation passed; compilation was blocked before C++ compilation by the open editor's active Live Coding. No portal Automation/PIE has run, and no portal material or map endpoint has been created. Existing map/corridor work was inspected and save-confirmed through MCP. The user stopped Computer Use with Escape. Resume from `CODEX_GOAL_CHECKPOINT.md`; this feature is incomplete and not accepted.
