# PHY-4 — actual-map native scene observation

Updated: 2026-09-29. Status: **SCENE REJECTION DIAGNOSED; PRODUCTION PASSAGE OPEN**.
Predecessor HEAD: `9ca52f5` on `portal/full-fidelity-p1`.

The existing read-only GT/PT binding bridge now carries a typed scene-issue mask
from the native clearance proof. The mask explains a fail-closed
`UnsupportedScene` without changing the permission decision. The verifier still
scans the complete native solver scene; it does not ignore contacting objects
because they are distant in the current image. A native integration speed cap
alone cannot bound collision-solver displacement or a persistent constraint.

`MapNativeSceneCoverage` opens a PIE copy of
`/Game/House/L_Interior_LivingKitchen` and creates a small physics box in front
of the authored blue portal. This test-owned body has no `PortalTraveller` tag,
is absent from the gameplay system's registered traveller list after discovery
and throughout observation, and is given a 250 cm/s native cap after creation.
Only the test owns its motion configuration. The legacy gameplay path retains
ownership of the authored cube; the new bridge reads state and never consumes a
proof, disables contacts, drives the box or commits a transfer. The test destroys
its runtime actor before ending PIE and does not save the map.

The authored scaled supports remain natively bound for three solver substeps:
`bindingMismatches=0`, final `DeltaSeconds=0.016666804`, and
`UnsupportedScene` with `sceneIssues=0x00000080` (`ActiveContact`). This means a
non-traveller active dynamic particle in the scene has retained native collision
contacts. The present independent-active lease explicitly rejects that state.
The diagnostic does not yet identify which actor owns the particle, so the
authored cube is a candidate, not a proven attribution. This run demonstrates
that native scene evaluation is reached and rejects safely; it does not prove
that the door can grant passage in the authored scene.

Next identify the contacting participant and its influence/contact island at
the solver boundary. Extend the clearance profile only with a defensible bound
on its contact-solver influence or keep that profile rejected. Then give the
production held/free traveller one owner for native speed cap, step budget,
motion intent and passage. The legacy PhysicsHandle/recovery/warp writers and
new writer must change over atomically. A scene diagnostic must not become a
reason to ignore a wall, contact or whole actor. Fact/ack transport and the
Core actual-map automated/manual PIE gates remain open.

Validation: UE 5.8 bundled .NET project generation and Development Editor
build **PASS**. Dedicated actual-map PIE Automation **1/1 PASS, zero test
warnings**. Affected `SlayTheSpireDemo.Interior.Portals.Physics` **46/46 PASS,
zero failures; one existing foundation test has two warnings**. This is an
automated physics-state gate, not manual visual acceptance. Evidence (ignored):
`Saved/Logs/PortalPhysicsPHY4MapNativeSceneFinalProjectFiles.log`,
`Saved/Logs/PortalPhysicsPHY4MapNativeSceneFinal3Build.log`,
`Saved/AutomationReports/PortalPhysicsPHY4MapNativeSceneFinal3/index.json`,
`Saved/AutomationReports/PortalPhysicsPHY4MapNativeScenePhysicsFinal/index.json`.
The user map was not saved; SHA-256 remains
`0436F682DCD867E43ECEA81E89D4916D8B8ECFB2E66CA9321997BC97038BE25C`.
