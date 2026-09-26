# PHY-2 — Chaos simulation boundary spike

Updated: 2026-09-26.
Status: **COMPLETE for the narrow profile below; PHY-3 NEXT; production migration and Core acceptance remain open**.
Source HEAD: `fb9d3f437e87c9778ce0c5d8459f83f8054b89ca`; this spike is an
uncommitted continuation. Authority and ordering:
[rigid-body contract](InteriorPortalRigidBodyInteractionDesign.md),
[Physics P1 execution](InteriorPortalPhysicsP1Execution.md).

## Purpose and isolation

Prove actual UE 5.8 / Chaos contact timing, bounded force application and a
restricted transfer interval before replacing gameplay authority. The mutating
callback and native world fixture live only in the editor test module. They are
never attached to `InteriorPortalSystem`, PhysicsHandle or legacy hold/gate/
Recovery writers. The runtime module supplies value-only protocol and geometry
helpers. The reported carrying oscillation is still open; this is not its fix.

## Declared profile

The native fixture uses a 1 kg dynamic box with 5 cm half-extents, initially at
`(8,0,0)` with velocity `(-120,0,0)` cm/s. Static support boxes have half-extents
`(2,100,150)` cm at X=0 and X=1000; their logical portal frames have +X normals.
The opening has half-width/height 65/115 cm with a 0.5 cm safety margin. A second
dynamic box at `(8,40,0)` retains ordinary support contacts, proving pair-specific
permission rather than ignoring the wall for everyone.

Gravity and linear/angular damping are zero; CCD is not enabled. There are no
joints, hold constraints or other contacts on the selected body. The authored
destination corridor is known to be clear. `ExitCorridorCertified` represents
that fixture assumption, not a production world-clearance query.

Game dispatch is 1/60 s. Physics configurations are one 1/60 s interval or two
1/120 s intervals, each in SingleThread and TaskGraph modes for contact tests.
The command permits at most 1/60 s and 2.01 cm translation per interval, fixed
orientation and a lease of one/two intervals respectively. The translation bound
includes the declared numerical allowance around nominal 2 cm movement.

Protocol rejects invalid/expired input, excessive duration/displacement, angular
motion, an invalid aperture interval, other contacts, or an uncertified crossing.
Native safe cancellation is proven for the normal approach/retreat fixture.
Lateral rim motion, rotation, high speed, arbitrary gravity, friction/energy
behavior, remote-half contacts and joint traversal are not accepted by this run.
PHY-3/PHY-4 must establish their supported profiles and safe rejection paths.

## Actual hooks and handoff

| UE/Chaos hook | Spike operation |
|---|---|
| `PreIntegrate` | Consume immutable command, validate binding/lease, cap and apply force; stop forbidden inward motion on a cancelled static approach before integration |
| `PostIntegrate` | Read actual P/Q and velocity; evaluate the same registered geometry and bounded interval |
| `ContactModification` | Examine generated contacts; reject other-body contacts; disable only the bound body/current support pair when eligible |
| `PreSolve` | Record that contact modification already occurred |
| `PostSolve` | Read solved state; commit one certified endpoint transfer through the physics-thread API and publish observations |
| `ParticleUnregister` | Permanently retire body/support binding before later use of a deallocated proxy |

Engine inspection used `PBDRigidsEvolutionGBF.cpp`, `PBDRigidsSolver.cpp`,
`Chaos/SimCallbackObject.h` and `PhysicsProxy/SingleParticlePhysicsProxy.h`.
Native samples prove phase order `1234` (pre-integrate, contact, pre-solve,
post-solve) in the tested profiles. Their keys contain a callback epoch, actual
`FPBDRigidsSolver::GetCurrentFrame()`, evolution serial and actual callback dt.
The two-substep runs produce distinct solver frames, not repeated Actor Tick IDs.
Repeated frames are rejected for permission/publication; nested/adaptive
evolution modes outside this tested partition are not accepted.

`FBoundaryCommand`, body states and receipts contain values, not UObject/world/
solver pointers. Native proxy bindings are installed separately before dispatch.
Callbacks do not invoke UObject APIs. A higher command revision starts a fresh
bounded lease; missing input never silently renews it. The UE simulation list
also invokes pre-simulate, so the spike has an intentionally empty override;
interval work starts at pre-integrate.

The fixture uses `StartFrame`, `WaitPhysScenes`, `EndFrame`, then the
timestamp-gated callback output queue. Waiting here is fixture completion, not a
recommendation to block production Actor Tick. Receipt validation checks current
body/pair/command/solver identities and monotonic interval ordering. The fixture
drains each dispatch before submitting the next revision; it does not prove
arbitrary asynchronous gameplay routing.

Receipt filtering is for advisory command results. **A committed transfer fact
must not disappear merely because a newer intent arrived.** PHY-4 must publish
durable transfer revisions and reconcile route/history independently of advisory
command freshness; `CanConsumeReceipt` is not that commit-fact protocol.

## Cancellation, transfer and lifetime findings

Restoring ordinary wall contacts alone was insufficient: the first native run
let the inserted box continue through the back of the wall (X=-13). The corrected
static cancellation is one boundary transaction: remove only forbidden inward
velocity and acceleration before integration, then let original wall contacts
solve the remaining penetration. No saved pose teleport is used. The pure
projection preserves tangent velocity and cannot increase kinetic energy; this
does not prove the native solver's broader restitution/friction energy contract.
The original native threshold X>6.8 cm was retained; it was not relaxed.

Transfer commits at the solved endpoint only for the isolated, fixed-orientation,
bounded interval with a certified empty destination corridor. Physics-thread
`SetX/SetR` also refresh P/Q; linear/angular velocity is mapped before the next
affected solve. There is no exit offset or game-thread actor warp. This does not
prove interval splitting at time of impact, blocked exits, sleep/inertia/joint
preservation or holder route rebinding. Those Core gates remain open. The spike
has no hold route and stops reapplying old-space force intent after transfer.

Destruction requires two guards. The owner submits cancellation with teardown;
the first affected input boundary permanently retires the binding before any
proxy read or force/contact use. Native unregister notification independently
retires it before delayed proxy deletion. The initial notification-only test
showed that the native notification could follow the first affected interval.
An old token replay cannot revive the retired binding; topology changes require
a new binding. Reset/replacement gameplay integration remains PHY-4 work.

## Actual validation

Prescribed bundled UE 5.8 .NET project generation **PASS**; Development Editor
build **PASS**. The editor test module adds only private built-in engine
`Chaos`/`PhysicsCore` dependencies. No third-party dependency, gameplay API,
renderer configuration or map asset changed. Initial compilation hit a test
constant name collision with an engine header; it was corrected before the final
build.

Focused NullRHI Automation:
`SlayTheSpireDemo.Interior.Portals.PhysicsBoundary` — **4/4 PASS, zero warnings**.
NullRHI disables rendering; these tests still execute real Chaos scenes.

| Test | Proven scope |
|---|---|
| `Protocol` | Bounds/rejection, force cap, static normal projection, stale/duplicate receipt rejection |
| `NativeContactAndSubsteps` | Four step/thread profiles; exact pair disabled, unrelated contacts retained, revoked permission restores blocking, lease expires |
| `NativeTransferAndRetirement` | TaskGraph/two substeps; one actual transfer, endpoint/velocity mapped, next interval at destination, cancellation/unregister/old replay safety |
| `NativeBoundedForce` | TaskGraph/one and two substeps; actual per-interval velocity change matches one capped force application |

Contact results (seven game dispatches each; disable/contact totals measured in
the first three dispatches):

| Substeps | Thread mode | Samples | Disabled selected pairs | Unrelated pairs retained | Final restored X (cm) |
|---|---|---:|---:|---:|---:|
| 1 | SingleThread | 7 | 3 | 3 | 7.000000 |
| 1 | TaskGraph | 7 | 3 | 3 | 7.000000 |
| 2 | SingleThread | 14 | 6 | 6 | 7.000001 |
| 2 | TaskGraph | 14 | 6 | 6 | 7.000001 |

The protected box remains on the blocked side (final X=10.605606/10.909869 cm).
These positions prove retained support response; they are not an energy result.

Force intent 100000 is capped to 1200 kg*cm/s² on the actual 1 kg body. Four
game dispatches produce four/eight native intervals and final VX=80.000008 cm/s
in both modes. Each actual interval has delta-V=`force / mass * dt`, tolerance
1e-4 cm/s; force norm tolerance is 1e-5. This bounds the adapter's force only;
hold torque/error bounds belong to PHY-3.

Transfer/lifetime run produces 20 samples, exactly one commit and a retired
binding. Mapped endpoint/velocity are compared with the actual solved state at
1e-6 tolerance; using rounded nominal speed 120 as an invariant was corrected.
The next substep continues outbound at the destination.

Final local evidence (ignored):
`Saved/AutomationReports/PortalPhysicsPHY2/index.json` (2026.09.26-12.50.22 UTC),
`Saved/Logs/PortalPhysicsPHY2Automation.log`. Failed development runs are retained
under `Saved/PortalPhysicsPHY2/`: `initial-fixture-crash.log` (missing fixture
completion/empty pre-simulate override), `first-native-failures.{log,json}`
(revocation, rounded-speed assertion and lifetime assumptions),
`retirement-failure.{log,json}` (notification-only retirement and missing world
context). Final fixture waits correctly, registers a world context and uses
owned components plus explicit owner cancellation. Failed runs are not passing
evidence. Unchanged PHY-1 evidence is reused.

No manual visual gate is required for this isolated nonproduction boundary
experiment. It is not PIE, visual or packaged-game acceptance. Future Core
physics/visual gates remain mandatory, with `USER ACTION REQUIRED` if required
manual acceptance cannot be executed.

## Next action

Implement PHY-3 bounded hold force/torque and shape-aware target/route solving
using the registered geometry and proven boundary. Define supported motion,
numeric force/error/energy criteria before actual Chaos runs. Keep experimental
writers isolated; production authority changes only in PHY-4 when superseded
hold/contact/recovery branches are removed together. Do not start P8/P9.
