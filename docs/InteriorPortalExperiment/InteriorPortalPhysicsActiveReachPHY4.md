# PHY-4 slice 2 — bounded independent active-body reach

Updated: 2026-09-28. Status: **NARROW ACTIVE PROFILE VALIDATED; general provider/cutover/Core OPEN**.
Source HEAD: `fd825aca848de01f120bf346badd0eab6f161bc8`; implementation is uncommitted.
Authority: [PHY-4 ordering](InteriorPortalPhysicsPHY4.md),
[rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md).

## Native proof boundary

The earlier certificate rejected every active dynamic particle that interacted
with the traveller, including a moving object far from either portal. This
increment admits a remote independent rigid body only when its native
`MaxLinearSpeedSq` is a usable finite cap, its native geometry has bounded
local bounds, and it has no persistent constraint, retained collision or CCD.
The default unbounded cap rejects. The cap limits Chaos integration velocity,
not contact or joint correction; it is therefore not a complete motion bound
for a contacting body.

At PreIntegrate, construct a whole-interval origin-centered reach from the
actual native bounds' maximum radius plus `sqrt(MaxLinearSpeedSq) * dt`.
Reject if it intersects either complete source/destination traveller corridor.
Independently scan every collidable native static shape against the reach with
the solver's collision-cull padding. Reject a potential contact before any
support contact may be disabled. Any other collidable non-static body except
the already bounded traveller lies outside this one-body profile. Keep the
4096-particle scene cap and 32 active leases / 16384 pair-check budget.

PostIntegrate and PostSolve revalidate native object state, geometry, speed cap,
contact-free topology and actual P/Q bounds inside the original reach. The
contact-modification boundary additionally checks every leased particle and
the actual modifier pairs before selectively disabling the traveller's own
support pair. A new contact, cap change, geometry change or lost participant
revokes the interval. Every interval starts with fresh leases; no game-thread
query or authored claim can issue them. The verifier still owns no movement or
contact writer and remains attached only to the editor TaskGraph fixture.

This deliberately excludes ordinary bodies resting on a floor while awake,
multiple mutually collidable moving bodies, awake contact/joint islands and
CCD. A finite speed cap alone must never authorize those cases. Native contact
and constraint response, topology mutation between callback stages, general
traveller motion/cancellation and real-world adapter assembly remain open.

## Evidence

Bundled UE 5.8 project-file generation and Development Editor build PASS. An
initial affected PhysicsSolverClearance run was 14/16: the new active fixture
passed, while two old tests assumed every far wake/active body must reject.
Those tests now explicitly configure the native unbounded cap for their denial
cases. A later 15/16 run exposed an unsafe timing assumption in a new fixture:
a collision became observable only after the contact callback. The provider
now rejects possible static contacts from the pre-integration reach instead
of relying on that callback for initial permission. The contact callback remains
an additional revocation guard.

Final rebuilt affected `SlayTheSpireDemo.Interior.Portals.PhysicsSolverClearance`
run: **16/16 PASS, zero test warnings/errors**, 2026-09-28 12:06 UTC.
The native one/two-substep held/free cases keep a far capped body moving while
one transfer fact commits. Capped reach near the exit, missing usable cap,
cap change after integration and a far static obstacle within the moving body's
reach all deny without support-pair suppression or a transfer. Existing
sleeping contact-island, wake, filter, kinematic, material and proof-lifecycle
regressions remain in that same final run.

Ignored evidence: `Saved/Logs/PortalPhysicsPHY4ActiveProjectFiles.log`,
`PortalPhysicsPHY4ActiveFinalBuild.log` and
`Saved/AutomationReports/PortalPhysicsPHY4ActiveVerified/index.json`.
The user-modified map was not edited; SHA256 remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.
No manual PIE was performed or required for this isolated provider increment.

## Next action

Extend the scene certificate from one independent active body to a bounded
interacting set or prove a safe native rejection/cancellation path for awake
contacts, joints, CCD and ordinary floor-supported motion. Then cover general
traveller motion and topology/fact handoff, assemble the real-world provider,
remove legacy writers atomically and run the actual-map Core physical and
visual gate. No production migration or Core acceptance is claimed here.
