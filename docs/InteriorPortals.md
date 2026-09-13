# Interior portal mechanism

Scope: `/Game/House/L_Interior_LivingKitchen`, including its existing corridor extension. This independent exploration feature does not advance battle phases.

Durable architecture and final Definition of Done: [`InteriorPortalFullFidelityImplementationPlan.md`](InteriorPortalFullFidelityImplementationPlan.md).

**Active execution/order authority for the current portal branch:** [`InteriorPortalCurrentExecutionPlan.md`](InteriorPortalCurrentExecutionPlan.md).

Current defect/evidence documents:

- [`InteriorPortalObservedIssues.md`](InteriorPortalObservedIssues.md)
- [`InteriorPortalPlayerTraversal.md`](InteriorPortalPlayerTraversal.md)
- [`Validation.md`](Validation.md)

## Intended behavior

One map-owned `InteriorPortalSystem` references blue/orange endpoints, an explicit list of portalable surfaces and the current physics traveller configuration. G equips/holsters portal input; LMB/RMB place blue/orange, R clears, E retains door/light interaction. The initial pair is accessible near the current corridor PlayerStart. Existing map and character work is preserved.

Rigid frames use +X outward, +Y horizontal, +Z vertical. Crossing transforms position, viewing orientation, linear velocity and rigid-body angular velocity through a 180-degree local Z rotation between endpoints. Swept front-to-back plane crossings avoid overlap-trigger teleports and speed-dependent missed planes. Closed/unpaired portals do not traverse. Placement requires a complete supported rim, an allowed planar surface, free opening and non-overlap. Moving or clearing a portal during traversal is rejected.

The player movement component constrains each CharacterMovement submove to a continuous legal capsule interval before bypassing its supporting primitive. Explicit crossing/clearance state prevents immediate reverse transfer until the capsule clears. Rigid-body travellers use the current prototype support-wall collision gate and remain subject to further Chaos hardening. Teardown restores movement ignores, constraints, presentation proxies and temporary lighting state. Shared wall collision is never disabled globally.

Scene captures run after the local player camera update. Full viewport aspect/projection is retained. Native SceneCapture clipping is the current production candidate, with guarded oblique projection retained for comparison/fallback. The logical aperture and cosmetic surface bias are separate. Finite deepest-first captures use HDR targets. The current P2 work is specifically responsible for making the capture/display exposure domain match the direct player view.

Implementation reference: [Froyok, Creating Seamless Portals](https://www.froyok.fr/blog/2019-03-creating-seamless-portals-in-unreal-engine-4/). The implementation uses original project code and generated procedural materials, without Valve game assets or third-party plugins.

## Current implementation state — `portal/full-fidelity-p1`, 2026-09-13

The portal branch is materially ahead of `main`. It contains the integrated full-fidelity candidate work and must be reviewed directly rather than inferred from the default branch.

Current branch capabilities include:

- logical portal plane separated from cosmetic surface bias;
- native SceneCapture clip-plane candidate with guarded oblique fallback;
- recursive HDR portal rendering;
- explicit player crossing/clearance state machine;
- per-submove aperture-constrained CharacterMovement;
- quaternion-owned portal camera/input state;
- runtime physics traveller registration/discovery;
- conservative sphere/capsule/box fit against the aperture;
- rigid-body position/orientation/linear/angular velocity transfer;
- held-object angular velocity preservation;
- player and rigid-body partial-crossing visual proxies;
- mapped first-person flashlight presentation;
- bounded portal-aware line traces and sphere sweeps with support-wall/aperture arbitration;
- focused Automation and actual-map traversal regression evidence.

The feature is **not yet Core Portal Fidelity sealed** and is **not Full Physics Fidelity complete**.

The immediate Core blocker is P2 capture/display exposure parity. Native clipping has mitigated the originally reproduced black-wall/diagonal-wedge failure in tested normal views, but the full near/grazing/recursion/replacement matrix still requires manual acceptance. Player traversal safety has substantially stronger automated/actual-map evidence, while arbitrary-orientation camera visuals, rigid-body high-speed Chaos behavior, player/held-item visual seams, flashlight visual quality, lifecycle stress and performance/VRAM profiling remain open.

The current execution plan therefore no longer follows the old prototype-stage order. It first closes Core Fidelity:

```text
exposure parity
→ integrated visual matrix
→ rigid-body Chaos hardening
→ PortalQuery completion
→ lifecycle closure
→ performance/temporal/VRAM gate
→ Core Portal Fidelity Seal
```

Only after that does the plan begin Full Physics work:

```text
P8A static-contact feasibility spike
→ remote collision ownership
→ mapped contact impulse/torque
→ finite-mass dynamic contacts
→ authority swap under contact
→ supported cross-portal constraint policy
→ Full Physics Fidelity Seal
```

The detailed reordered stages and acceptance gates live in [`InteriorPortalCurrentExecutionPlan.md`](InteriorPortalCurrentExecutionPlan.md).

## Acceptance

AUTOMATED GATES: UE 5.8 project generation and Development Editor build; focused `SlayTheSpireDemo.Interior.Portals` tests; saved-map pair/material/surface load checks; traversal, velocity, placement and cleanup assertions; PortalQuery/traveller regressions; high-speed Chaos tests as that path is hardened.

MANUAL PIE GATES: moving-view parallax, both travel directions, near-plane transition, grazing-angle stability, facing-portal recursion, wall/floor/ceiling camera continuity, rigid-body angular momentum, held-object passage, partial-crossing visuals, flashlight continuity and final remote-contact behavior when Track B exists.

Single local player is the current product scope. Finite recursion and SceneCapture-specific temporal/lighting behavior must be assessed explicitly; C++ tests do not prove visual parity, Chaos contact quality or packaged behavior.
