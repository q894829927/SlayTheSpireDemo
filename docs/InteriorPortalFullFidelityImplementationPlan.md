# Interior Portal Full-Fidelity Implementation Plan

Date: **2026-09-12**

Status:

```text
DEDICATED IMPLEMENTATION PLAN /
CURRENT CORE PORTAL LOOP IMPLEMENTED /
VISUAL + PHYSICS HARDENING REQUIRED /
NOT YET FULL-FIDELITY ACCEPTED
```

Related contracts and current references:

- `AGENTS.md`
- `docs/InteriorPortals.md`
- `docs/ValidationExecutionPolicy.md`
- `docs/Validation.md`
- `docs/CODEX_GOAL_CHECKPOINT.md` only for resumable execution state, not as design authority
- `Source/SlayTheSpireDemo/Interior/InteriorPortalMath.h`
- `Source/SlayTheSpireDemo/Interior/InteriorPortal.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalSystem.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalCameraManager.h/.cpp`
- `Source/SlayTheSpireDemoTests/Private/InteriorPortalTests.cpp`
- `tools/setup_interior_portals.py`
- `tools/validate_interior_portals.py`

The implementation inventory in this plan was reviewed against current `main` on 2026-09-12. Do not treat the document commit itself as a permanent code baseline. When execution starts or resumes, record the exact implementation HEAD in `docs/CODEX_GOAL_CHECKPOINT.md` and verify the inventory against that HEAD.

This initiative is independent from the card-battle phase plan. It must not change battle architecture, gameplay authority, UI phase state or retained Legacy UI policy.

---

## 0. Review-driven design corrections

This revision keeps the original architecture and feature scope, but tightens several places where the previous plan described the desired result more strongly than the current implementation path could guarantee.

The following corrections are intentional:

1. **Native SceneCapture clip planes are preferred, not assumed to be free or universally superior.** P1 now has a correctness-and-performance gate before making the native clip path the production path.
2. **Camera continuity needs an explicit quaternion-owned transient view state.** Mapping a quaternion and immediately converting it back into ordinary `ControlRotation` is not sufficient for arbitrary wall/floor/ceiling transitions if roll is simultaneously being recovered.
3. **Aperture-local collision must use swept/substep-aware gate entry and exit.** A per-frame broad support ignore alone cannot guarantee that a fast traveller will not leave the aperture laterally between checks.
4. **Rigid-body fit is renamed from "exact fit" to geometry-aware conservative fit.** Representative samples are only exact when the supported collision geometry and proof are exact; the production contract should not overstate approximation quality.
5. **Dual-space contact bridging gets a feasibility spike before full implementation.** Chaos callback timing, friction fidelity and impulse duplication are high-risk enough to prove on one constrained case before building the complete bridge.
6. **Portal-aware queries must analytically compete with ordinary world hits.** The visual portal surface currently has no collision, so a normal world trace cannot rely on "hitting the portal" first.
7. **The project gets two useful acceptance seals.** Core portal fidelity can be accepted without pretending that experimental cross-portal constraint/contact bridging is already production ready.

These changes reduce implementation ambiguity and make failure states explicit instead of allowing an approximation to satisfy a stronger acceptance statement accidentally.

---

## 1. Objective

Deliver a production-quality, Portal-like two-endpoint spatial connection for the interior exploration map with seamless visual continuity, traversal continuity and rigid-body continuity.

Within this project, **full fidelity** means all of the following are true at the same time:

```text
camera view through portal
+ player traversal
+ rigid-body traversal
+ carried-object traversal
+ partial-crossing appearance
+ collision passage
+ portal-aware world queries
+ recursion
+ stable near-plane behavior
+ consistent lighting/exposure
+ deterministic cleanup and lifecycle
```

The player should perceive the two portal apertures as one continuous opening in space rather than as a television texture followed by a teleport.

The target is behavioral and visual fidelity using original project code and UE 5.8 facilities. This plan does **not** require Valve assets, copied Valve code, proprietary shaders or pixel-for-pixel reproduction of a proprietary renderer.

Primary supported product scope is the existing **single-local-player UE 5.8 exploration implementation**. Multiplayer replication is explicitly outside this plan unless separately authorized.

---

## 2. Acceptance levels

The implementation is not accepted merely because teleportation works. Acceptance is split into two levels so the project can reach a stable production-useful portal system before optional/high-risk dual-space physics is sealed.

### 2.1 Core portal fidelity seal

The core seal requires:

```text
stable portal rendering
+ continuous player crossing
+ ordinary rigid-body transfer
+ Physics Handle continuity
+ partial-crossing visual slicing
+ aperture-local collision passage
+ portal-aware line/sweep queries
+ deterministic lifecycle cleanup
+ bounded recursion
```

It does **not** claim physically correct destination-side contact for the remote half of a partially crossed body, nor general cross-portal multi-body constraint solving.

### 2.2 Full physics fidelity seal

The full physics seal additionally requires:

```text
remote-half destination collision/contact
+ mapped contact impulse/torque response
+ stable authority swap
+ the documented supported constraint policy
```

If the core seal passes while dual-space contact bridging remains deferred, the project status should say **CORE PORTAL FIDELITY SEALED / FULL PHYSICS FIDELITY DEFERRED**, not "full-fidelity complete".

### 2.3 Visual continuity

- Looking through either portal shows the paired portal's space from the correct transformed eye position and orientation.
- Parallax remains correct while moving laterally, vertically, toward, away from and across the aperture.
- Portal contents remain aligned with the physical aperture at center view, edge view and grazing angles.
- Geometry behind the exit support plane never leaks into the portal image.
- No giant triangles, diagonal wall wedges, near-plane flashes, one-frame black frames or stale render targets are visible.
- The transition from looking through a portal to physically crossing it has no visible camera jump.
- Recursive portal views remain spatially coherent up to the configured finite recursion depth.
- Portal rendering applies exposure/tone mapping exactly once and does not visibly diverge from the destination room solely because it is seen through a portal.
- Local portal rim art is allowed to be stylized, but the scene inside the aperture must remain spatially faithful.

### 2.4 Player traversal continuity

- Crossing works wall-to-wall, wall-to-floor, floor-to-wall, floor-to-ceiling and arbitrary valid portal orientations within the declared character/gravity policy.
- Position, camera orientation and linear momentum map through the same rigid portal transform.
- Entry speed magnitude is preserved except for ordinary game movement rules applied after exit.
- High-speed crossings cannot tunnel past the portal plane.
- The player cannot cross if the full capsule does not fit the aperture or the exit pose is blocked.
- The player cannot escape through the supporting wall outside the legal portal opening.
- Crossing cannot immediately retrigger the exit portal in a ping-pong loop.

### 2.5 Rigid-body continuity

- Position, body orientation, linear velocity and angular velocity map correctly.
- Momentum magnitude is unchanged by equal-scale portals.
- A rotating object exits with the correctly transformed rotation axis.
- High-speed rigid bodies cannot miss the crossing.
- A rigid body that does not fit the aperture cannot pass.
- A partially inserted object is visually split between spaces without duplicate visible geometry.
- Full physics fidelity additionally requires a remote collision representation so the portion visible through the remote portal can participate in destination-side contacts before center-plane authority transfer.
- Crossing while held by a Physics Handle remains stable and cannot produce an impulse explosion.

### 2.6 Interaction continuity

Portal-aware world queries must support at least:

```text
interaction traces
Physics Handle targeting
weapon / projectile traces used by this exploration mode
portal placement queries where intentionally allowed
future laser/beam-style queries without rewriting portal math
```

A query that enters one portal can continue from the paired portal with transformed origin/direction and a finite recursion limit.

### 2.7 Lifecycle and robustness

- Moving, clearing or replacing a portal cannot strand ignored collisions, constraints, proxies or render targets in an invalid state.
- Destruction/unregistration of a traveller during crossing is safe.
- Reset returns all temporary collision state to normal.
- No portal code depends on unstable actor discovery order.
- Runtime behavior is bounded by explicit recursion/query limits.

---

## 3. Current implementation inventory

The current implementation already contains a substantial part of the required foundation and must be evolved rather than replaced wholesale.

### 3.1 Rigid portal mathematics — IMPLEMENTED

`InteriorPortalMath` currently provides:

- rigid entry-to-exit rotation using a local 180-degree Z rotation;
- world-position mapping through a portal pair;
- conservative elliptical aperture membership tests;
- continuous front-to-back segment/plane crossing detection;
- a reversed-Z oblique projection helper.

The existing rigid transform preserves handedness and vector magnitude for equal-scale portals. Focused tests already cover round-trip position mapping, momentum magnitude, inward-to-outward normal mapping, handedness and high-speed swept crossing.

### 3.2 Portal endpoint actor — IMPLEMENTED

`AInteriorPortal` currently owns:

- portal surface mesh;
- `USceneCaptureComponent2D`;
- dynamic portal material;
- portal color;
- explicit support primitive;
- runtime HDR render targets;
- finite recursion target allocation;
- linked/unlinked view state.

Current capture configuration intentionally disables motion blur, Temporal AA, bloom and eye adaptation and stores scene-linear HDR color in `RTF_RGBA16f` targets.

### 3.3 Portal placement — IMPLEMENTED BASELINE

Current placement logic already contains:

- allowed portal surface filtering;
- hit-normal-derived frame construction;
- a complete-rim sampling check;
- opening-space validation;
- overlap rejection;
- explicit blue/orange endpoint ownership;
- protection against moving/clearing portals while a traversal is active.

### 3.4 Player traversal — IMPLEMENTED BASELINE

Current traversal already uses:

- previous/current eye position rather than overlap-only teleport;
- swept front-to-back portal plane detection;
- capsule fit checks against the elliptical aperture;
- transformed camera/control orientation;
- transformed CharacterMovement velocity;
- exit overlap rejection;
- `TeleportPhysics` placement;
- last-exit tracking to avoid immediate retrigger;
- temporary support collision ignore while the capsule occupies the valid passage region.

Known hardening gap: exit blocking currently evaluates an upright capsule/world-space orientation rather than a separately owned arbitrary portal camera orientation. The final character/root orientation policy therefore needs to stay distinct from the camera-view policy.

### 3.5 Rigid-body traversal — IMPLEMENTED BASELINE

Current physics traversal already contains the core state required for correct basic rigid-body transport:

- physics traveller tracking;
- previous-position crossing detection;
- transformed body position and orientation;
- transformed linear velocity;
- transformed angular velocity for ordinary rigid-body traversal;
- `TeleportPhysics` transfer;
- destination-side exit tracking;
- pairwise collision suppression against the portal support using a free Chaos constraint rather than globally disabling the wall.

Known hardening gap: final exit occupancy still uses a coarse bounding-sphere approximation rather than collision-geometry-aware fit/overlap.

### 3.6 Held-object support — PARTIALLY IMPLEMENTED

The system already has a `UPhysicsHandleComponent` path that:

- transforms held target location through the portal;
- transforms target rotation;
- ignores the relevant support wall during cross-portal targeting;
- prevents the handle target from treating the two spaces as ordinary distant world points.

This path still requires dedicated angular-velocity preservation, shared portal-query usage and crossing-state hardening described later in this plan.

### 3.7 Partial-crossing visual proxy — IMPLEMENTED BASELINE

Current initialization creates a no-collision proxy StaticMesh for configured physics travellers and dynamic materials for source/proxy slicing.

`M_PortalTestCube` already exposes slice origin/normal/enabled material parameters so the source and remote visual representations can be clipped against the portal plane.

This currently provides **visual continuity only**. The remote proxy has `NoCollision`, so destination-side physical contact during partial crossing is not yet represented.

### 3.8 Recursive rendering — IMPLEMENTED BASELINE

`RenderViews` already:

- uses the local player's actual viewport projection;
- computes a transformed virtual view for each recursion level;
- renders deepest-first into separate render targets;
- hides the remote endpoint actor from its own capture;
- uses a finite configured recursion depth;
- supports Lumen GI/reflection overrides for capture;
- presents the top render target through the portal surface material.

### 3.9 Existing validation — PARTIALLY IMPLEMENTED

Focused C++ Automation currently covers:

- rigid transform invariants;
- high-speed crossing math;
- aperture rejection;
- reversed-Z oblique clipping math;
- basic endpoint lifecycle and placement guard behavior.

Setup/validation Python scripts exist for authoring the test pair, materials, demo surface and physics cube in the interior map.

---

## 4. Current blocking visual defect

The current PIE result has a reproducible rendering defect: near or oblique views of the blue portal can show a large black wall region or a giant diagonal triangular wedge inside the portal image.

The current capture constructs an oblique near plane from the exit portal and injects it into the player's projection matrix. The clip plane position is currently approximately:

```cpp
ExitLocation + ExitForward * 0.1f
```

The observed artifact is consistent with the exit support mesh intersecting or leaking across that custom oblique near plane, especially at grazing camera angles.

The existing implementation has four risk factors that must be treated as one blocking rendering investigation:

1. **clip plane offset is extremely small** relative to support mesh thickness / triangle placement;
2. **custom oblique projection can become numerically ill-conditioned** when the plane/camera relationship approaches a grazing configuration;
3. **the support wall remains renderable**, so correctness depends entirely on the clipping plane rather than on explicit support suppression;
4. SceneCapture currently uses both a custom projection matrix and a custom near-clipping-plane override, producing two near-plane mechanisms that should not be allowed to drift independently.

Additionally, the current fallback behavior of returning the unmodified projection when the oblique denominator is invalid effectively removes portal clipping for that frame. A hardened fallback must never silently convert numerical failure into a visibly unclipped capture.

This defect blocks any claim of seamless visual parity.

---

## 5. Architecture target

The final system should be organized around a small number of reusable portal services rather than special cases inside one tick function.

```text
AInteriorPortal
    endpoint frame / aperture / support / render surface

AInteriorPortalSystem
    pair lifecycle / placement / routing / high-level orchestration

PortalMath
    rigid transform + aperture geometry

PortalRenderBridge
    virtual cameras / clip plane / recursion / render targets

PortalCameraState
    transient quaternion view orientation / crossing / horizon recovery

PortalTraveller state
    previous pose / crossing state / support gating / proxy state

PortalQuery
    analytic portal intersections + recursive line/sweep routing

PortalPhysicsBridge
    authoritative rigid body + remote proxy + contact mapping
```

This does not require all names above to become separate UObject types. The required architectural rule is that **the same mapping primitives are reused by rendering, traversal, physics and queries** so those systems cannot silently implement different portal transforms.

---

## 6. Shared transform contract

All portal subsystems must use one rigid mapping contract:

```text
world entry frame
    -> entry-local coordinates
    -> local 180-degree turn
    -> exit-world coordinates
```

Required reusable operations:

```cpp
MapPosition
MapDirection
MapRotation
MapTransform
MapLinearVelocity
MapAngularVelocity
MapPointAndNormal
MapRay
```

Rules:

- use no-scale transforms for momentum vectors;
- portal scale is not allowed to modify mass or momentum in this implementation;
- position/orientation/velocity/query mapping must all use the same quaternion basis;
- mapping A->B followed by B->A must return the original state within a strict tolerance;
- tests must cover wall, floor, ceiling and arbitrary rotations.

---

## 7. Delivery stages

Stage numbers identify feature areas. **They are not the final execution order.** Section 14 defines the execution order and intentionally runs P10 before P6/P7 so later interaction and Physics Handle work does not accumulate more one-off trace logic.

### P0 — Freeze baseline and instrument the defect

**Goal:** create a reproducible before/after baseline before changing the renderer.

Required work:

- record current HEAD and current portal setup asset/map paths;
- add temporary debug drawing/toggles for:
  - entry/exit portal plane;
  - current capture camera transform;
  - current clip plane normal/base;
  - recursion level;
  - currently ignored support;
- capture a small manual matrix of portal views:
  - centered far;
  - centered near;
  - left/right edge;
  - high/low edge;
  - grazing angle;
  - during camera crossing;
- preserve the current bad screenshot as regression evidence in the implementation notes/validation record if project policy allows image evidence.

Acceptance:

- the triangular/wall-leak artifact can be reproduced intentionally before the fix;
- debug data identifies which exit plane/support is involved.

No gameplay behavior change is authorized by P0.

---

### P1 — Replace fragile portal clipping with a stable render clip path

**Goal:** remove the current black-wall/giant-triangle artifact without changing portal mapping.

Preferred implementation candidate:

1. enable UE project support required for SceneCapture clip-plane rendering;
2. use `SceneCaptureComponent2D::bEnableClipPlane`;
3. set:

```cpp
ClipPlaneBase   = ExitPortalLocation + ExitForward * ClipBias;
ClipPlaneNormal = ExitForward;
```

4. retain the player's projection matrix for matching FOV/aspect, but remove oblique near-plane mutation from the normal native-clip candidate path;
5. disable the redundant `bOverride_CustomNearClippingPlane` path unless an independently justified camera near clip is required;
6. keep the exit portal actor hidden from its own capture;
7. do **not** permanently hide the entire support wall as the final solution, because doing so can create a visually oversized hole beyond the aperture.

`ClipBias` must be data-driven/tunable and chosen from portal/support geometry, not hard-coded as a magic large distance. Start with a minimal centimeter-scale bias and validate at all view angles.

#### P1 production-path gate

Native clipping becomes the production path only if all of the following are true:

- it passes the complete P0 visual matrix;
- Lumen/reflection/fog/translucency behavior remains acceptable;
- enabling the required renderer/project setting does not create an unacceptable project-wide rendering cost on the target hardware;
- editor/shader rebuild implications are recorded in the implementation notes;
- recursion depth >= 2 remains visually correct.

**Why this gate exists:** native clipping removes fragile projection surgery, but it changes renderer configuration outside the portal actor itself. The plan should therefore compare correctness and measured cost instead of assuming that the native path is automatically free.

Fallback if native SceneCapture clip planes fail the production gate:

- keep the custom oblique path behind one explicit implementation switch;
- normalize/orient the view plane consistently;
- classify invalid/ill-conditioned denominators using a meaningful configured epsilon and sign rule rather than `UE_SMALL_NUMBER` alone;
- never emit an unclipped projection as a silent numerical fallback;
- either preserve a previously validated matrix for a bounded case or deliberately skip/blank the affected capture with diagnostics while the issue is investigated;
- add extreme near/grazing unit tests;
- remove the second competing near-clipping mechanism.

Acceptance:

- no support-wall leak in the full P0 view matrix;
- no diagonal giant triangle at grazing angles;
- no one-frame flash when camera approaches or crosses the portal plane;
- projection alignment remains unchanged;
- selected production clip mode and measured reason are documented.

This is the first blocking delivery and should land before expanding physics.

---

### P2 — Camera/projection and render-target fidelity

**Goal:** make the portal image match the destination scene as closely as the UE SceneCapture path permits.

Required work:

- copy the player's effective FOV/aspect/constrained viewport projection exactly;
- verify off-center/constrained viewport behavior;
- ensure render target size tracks the constrained view rect and only reallocates when required;
- define the single authoritative post-process/exposure contract;
- preserve scene-linear capture and apply exposure exactly once;
- audit Lumen GI/reflections through SceneCapture;
- explicitly decide Temporal AA / TSR behavior rather than leaving it disabled only as an artifact workaround;
- verify bloom, auto exposure, fog, translucent materials, decals and emissive sources through the portal;
- add a portal-surface depth bias only if required to eliminate z-fighting without creating a visible depth discontinuity;
- verify no feedback loop from a portal capturing its paired recursive surface.

Target rule:

```text
same destination point viewed directly
vs
same point viewed through portal
```

must not show an unexplained exposure/color-space discontinuity.

Acceptance:

- side-by-side direct/portal destination screenshots have no obvious exposure doubling, black crush or gamma shift;
- moving camera does not cause stale target or visible render-target resize flashes;
- recursive view remains aligned at every enabled depth.

---

### P3 — Camera crossing and player traversal hardening

**Goal:** remove all discontinuity around the instant the eye/capsule crosses the portal plane.

Required crossing state:

```text
Outside
ApproachingEntry
IntersectingAperture
Transferred
ClearingExit
```

Required work:

- make camera-view mapping and physical traversal use the exact same crossing frame;
- retain continuous previous/current eye segment detection;
- use hysteresis to avoid plane-chatter around `X ~= 0`;
- preserve transformed velocity before/after `TeleportPhysics`;
- introduce an explicit transient **portal camera orientation state** owned by the camera/controller path;
- store the crossing view as a quaternion and keep quaternion composition authoritative while crossing/clearing;
- do not rely on `FRotator -> SetControlRotation -> immediate roll recovery` as the sole representation of the mapped view;
- only begin optional horizon/roll recovery after the camera is safely in `ClearingExit` or later;
- define the recovery policy as comfort behavior, not as portal-space math;
- keep Character actor-root orientation policy explicit:
  - upright capsule/world-gravity behavior remains controlled by CharacterMovement unless a later gravity system is separately authorized;
  - camera orientation may carry transformed pitch/yaw/roll during the crossing;
  - actor root yaw policy and camera quaternion policy are separate contracts;
- make exit blocking use the actual target capsule pose required by the chosen character-root policy;
- make exit offset minimal and derived from collision clearance.

**Why this change is better:** the existing implementation transforms a quaternion but then immediately re-enters an Euler/control-rotation path where roll is interpolated toward zero. An explicit transient quaternion state prevents the visual portal transform from being partially undone during the exact frames where continuity matters most.

Acceptance scenarios:

- walk through slowly;
- sprint through;
- jump/fall through;
- floor->wall launch;
- wall->floor fall;
- ceiling-related orientation where placement is allowed;
- enter while looking sharply up/down/sideways;
- repeatedly alternate portals without camera snap or retrigger;
- no roll snap on the transfer frame; any later horizon recovery is intentional and visible only after clearance.

---

### P4 — Aperture-local collision passage instead of broad wall ignore

**Goal:** allow traversal through the portal hole while preventing escape through the rest of the support surface.

Current behavior temporarily ignores the whole support primitive when the character/body is close enough and fits the aperture. This is safe as a prototype but not full-fidelity collision.

Target behavior:

- support collision is bypassed only while the traveller occupies the legal portal prism and is committed to passage;
- **entry into and exit from the legal prism are swept/substep-aware**, not only sampled once per game frame;
- leaving the aperture laterally immediately restores normal support collision before the traveller can cross the support outside the legal opening;
- restore uses a safe non-penetrating pose or depenetration path;
- cleanup on portal clear/destroy always restores collision.

Implementation options, in preferred order:

1. aperture-gated collision state with swept/substep validation and immediate restore;
2. segmented/custom support collision that exposes an actual portal-shaped opening when the asset topology permits;
3. low-level Chaos contact modification only if higher-level solutions cannot provide robust aperture-local filtering.

A game-thread Tick-only support ignore is acceptable as an intermediate prototype, not as the final P4 acceptance mechanism for high-speed rigid bodies.

**Why this change is better:** per-frame membership can miss a fast lateral escape between samples. Swept/substep state transitions make the collision contract match the same continuous-crossing philosophy already used for portal-plane traversal.

Acceptance:

- player/body can pass through the portal center;
- the same traveller cannot move sideways through the support wall beside the portal while passage gating is active;
- high-speed diagonal motion cannot enter through the aperture and leave through solid support between gate checks;
- closing/moving a portal cannot leave a permanent collision hole.

---

### P5 — Generic traveller model and geometry-aware conservative body fit

**Goal:** stop treating portal-capable physics bodies as an editor-authored fixed list with bounding-sphere-only fit.

Introduce a reusable traveller representation, preferably a lightweight component or interface, responsible for:

```text
registration
previous pose
entry/exit state
source/remote visual proxy
support gate state
physics body reference
cleanup
```

Requirements:

- dynamic runtime-spawned physics actors can register automatically;
- destroyed actors unregister safely;
- explicit ordering is used where iteration order matters;
- existing configured demo cube continues to work.

Replace `Bounds.SphereRadius` as the final aperture-fit authority.

Geometry-aware conservative fit strategy:

- **sphere:** analytic radius inset against the aperture;
- **capsule:** project segment endpoints plus radius/support extent in portal-local coordinates;
- **box:** transform all 8 oriented vertices and reject any required support extent outside the inset aperture;
- **convex:** use all available convex vertices or a proven support-function equivalent;
- **compound body:** evaluate every collision primitive required to pass the aperture;
- keep a small configurable safety margin around the rim.

Do not call a finite representative-point approximation "exact" unless its supported geometry and proof make it exact.

**Why this change is better:** a bounding sphere rejects long thin objects that really fit, while sparse samples can accept shapes that actually clip the rim. Naming the contract "geometry-aware conservative" accurately describes a safe production test and lets each supported collision primitive use an appropriate proof/approximation.

Acceptance:

- long thin object can pass in an orientation that truly fits;
- the same object is rejected in an orientation that does not fit;
- sphere/cube behavior does not regress;
- compound shapes cannot pass merely because their root/bounds center fits.

---

### P6 — Physics crossing hardening

**Goal:** make ordinary complete rigid-body transfer stable for all supported velocities/orientations before adding two-world contact bridging.

Requirements:

- perform crossing detection using previous/current physics pose with substep awareness where necessary;
- enable/test CCD for high-speed travellers but do not rely on CCD alone for portal plane detection;
- transform:

```text
position
orientation
linear velocity
angular velocity
```

through the same portal quaternion;
- preserve sleep/awake state intentionally;
- preserve mass/inertia for equal-scale portals;
- avoid applying displacement-derived fake velocity;
- verify destination overlap using geometry-aware collision rather than the old reduced bounding sphere where required;
- protect against repeated transfer in the same simulation interval.

Held-object requirements:

- use the P10 portal-query layer for remote targeting rather than adding another bespoke trace path;
- snapshot linear and angular velocity before any release/re-grab sequence;
- transform both velocities;
- transform Physics Handle target location and orientation;
- rebind without a large handle error impulse;
- cleanly handle the player crossing while the object is still on the opposite side.

Acceptance:

- thrown cube preserves speed;
- spinning cube preserves transformed rotation axis;
- thrown spinning cube remains stable;
- held cube can be pushed halfway through, player can cross, and grab remains stable;
- no explosive impulse or NaN/invalid transform.

---

### P7 — Partial-crossing visual continuity for every traveller

**Goal:** generalize the existing cube slice prototype into production partial-crossing visuals.

Required work:

- source and remote proxy use the same mesh/material slots;
- create dynamic material instances for every relevant slot, not only material index 0;
- support meshes whose production material cannot directly accept slice parameters by using a documented portal-slice material function/path;
- source mesh clips the remote half;
- remote proxy clips the source half;
- proxy transform is updated every frame from the source through the portal mapping;
- shadow/cast-shadow policy is explicit to prevent duplicate shadows;
- proxy never appears when the object is not intersecting the portal slab;
- portal relocation/reset destroys or hides stale proxies.

Character/weapon presentation:

- first-person weapon/hand meshes must not visibly intersect the portal plane incorrectly;
- if a visible character body is present, apply the same split/proxy principle or explicitly hide body regions that would otherwise reveal the teleport.

Acceptance:

- object pushed slowly through portal has no pop when center crosses;
- no double-rendered whole object;
- no missing middle slab;
- no duplicate shadow obvious at the aperture.

Passing P0-P7 plus the executed P10 query layer and required lifecycle/performance gates is sufficient for the **Core Portal Fidelity Seal** defined later.

---

### P8 — Dual-space collision/contact bridge

**Goal:** make the remote half of a partially crossed rigid body physically interact with the destination world before the authoritative body center transfers.

This is the largest remaining physics feature and must be attempted only after the core portal path is stable.

Target model:

```text
Source world
    authoritative Chaos body
           |
           | rigid portal mapping
           v
Destination world
    remote collision proxy
```

The remote proxy is not independent gameplay state. It is a physical projection of the authoritative body.

#### P8A — Contact-bridge feasibility spike

Before building the complete bridge, prove one deliberately narrow case:

```text
one dynamic cube
half inserted through one portal
remote kinematic/contact-reporting proxy
one static destination wall
one remote normal contact
mapped impulse applied back to authoritative body at mapped source contact point
```

The spike must answer:

- can the required contact data be obtained at a deterministic enough point in the Chaos simulation lifecycle;
- can one remote impulse be mapped back exactly once without a one-frame duplicate or feedback loop;
- does applying the mapped impulse at the mapped contact point generate plausible linear and angular response;
- can sleep/wake state remain stable;
- can proxy/support collision filtering avoid blocking legal passage;
- is an engine-level/Chaos contact hook required, or are project-level callbacks sufficient.

P8A acceptance:

- remote wall contact visibly pushes the source body back;
- oblique contact produces torque in the correct mapped direction;
- no duplicate impulse, NaN, runaway energy or persistent proxy remains after cleanup;
- the implementation layer required for the full bridge is documented.

**Go/no-go rule:** do not implement friction, movable-vs-movable contact or authority swapping until P8A passes. If P8A cannot provide stable contact timing without disproportionate engine-level complexity, stop at the Core Portal Fidelity Seal and document full physics fidelity as deferred.

**Why this change is better:** the hardest P8 problem is not the portal quaternion math; it is contact timing and ownership inside Chaos. A narrow spike prevents a large remote-physics framework from being built on top of an unsuitable callback path.

#### P8B — Full dual-space bridge

After P8A passes, required behavior is:

- destination-side proxy has collision enabled only while the source body intersects the portal slab;
- proxy pose/velocity are continuously mapped from the authoritative body;
- proxy does not collide with its paired support wall/rim in a way that prevents legal passage;
- destination contact impulse is mapped back to source coordinates;
- mapped impulse is applied at the mapped source contact point so both linear and angular response are represented;
- friction/tangential response is validated, not only normal impulse;
- gravity is not applied twice;
- source and proxy cannot collide with each other through unrelated broad-phase overlap;
- when the body center crosses, authority swaps atomically to the destination representation and the former source side becomes the proxy until the body fully clears.

Implementation preference:

1. begin with the P8A contact-reporting proxy and explicit impulse mapping;
2. if UE-level callbacks cannot provide sufficient contact/friction fidelity, introduce a narrowly scoped Chaos bridge inside the project;
3. do not add a third-party physics dependency.

Required impulse mapping math:

```text
remote contact point -> inverse portal map -> authoritative point
remote impulse       -> inverse direction map -> authoritative impulse
remote normal        -> inverse direction map -> authoritative normal
```

Torque must arise from applying the mapped impulse at the mapped contact point rather than only changing center-of-mass velocity.

Acceptance scenarios:

- half-inserted cube collides with a wall in the exit room and is pushed back;
- remote collision with another movable cube transfers momentum plausibly;
- rotating/oblique contact produces angular response;
- friction/tangential response is stable enough for the supported use case;
- no energy explosion when authority swaps;
- repeated passage does not accumulate proxy bodies.

---

### P9 — Constraints and compound physical relationships

**Goal:** define predictable behavior for objects that are not isolated rigid bodies.

Cases:

```text
Physics Handle
Physics Constraint
attached components
compound actors
stacked contacting bodies
```

Required policy:

- Physics Handle is supported as part of P6;
- attached visual-only children follow the authoritative actor transform;
- for actual multi-body constraints, either:
  - map both constrained bodies when the whole assembly crosses together; or
  - bridge constraint anchors through the portal while bodies occupy opposite spaces.

Do not silently break a constraint or teleport one constrained body with no documented behavior.

For the Core Portal Fidelity Seal, unsupported multi-body constrained traversal may be **explicitly rejected** rather than bridged. Cross-portal constraint bridging is required only for the Full Physics Fidelity Seal if the project chooses to claim that support.

---

### P10 — Portal-aware world-query layer

**Goal:** make a portal a real spatial connection for traces and interaction, not only for cameras and bodies.

Provide reusable bounded query functions equivalent to:

```cpp
PortalLineTrace(..., MaxPortalDepth)
PortalSweep(..., MaxPortalDepth)
```

#### Required line-trace algorithm

The portal surface mesh currently does not provide collision and therefore cannot be treated as an ordinary hit target. Each query segment must compare two independently computed candidate events:

```text
1. ordinary world trace -> nearest blocking world hit distance (if any)
2. analytic ray/segment vs each valid portal plane
       -> front-side eligibility
       -> plane intersection distance
       -> aperture ellipse membership
       -> portal state/link validity

choose the nearest valid event

world hit nearest:
    return ordinary hit

portal intersection nearest:
    consume travelled distance
    map intersection point + direction through pair
    apply tiny forward epsilon in exit space
    continue query
    decrement recursion budget
```

For sweeps, the same competition applies, but portal eligibility must use the swept shape/support extent rather than a zero-radius ray aperture test.

**Why this change is better:** relying on a normal trace to "hit the portal" first is incompatible with the current no-collision portal surface. Analytic portal-plane competition also makes query behavior independent from decorative portal mesh collision settings.

Requirements:

- ordinary hit distance and portal-plane distance are compared in the same segment;
- prevent immediate self-rehit at the exit plane;
- finite recursion/cycle protection;
- preserve ignored actors/components correctly across each segment;
- return a segmented debug path for diagnostics;
- preserve total remaining distance across hops;
- support sweep radius/shape where Physics Handle targeting needs volume rather than a ray.

Adopt it for:

- E interaction where appropriate;
- Physics Handle targeting;
- exploration weapon/projectile/beam traces where present;
- any later laser puzzle mechanic.

Portal placement shots may opt in or out as an explicit gameplay rule; they must not accidentally inherit behavior from an unrelated trace implementation.

Acceptance:

- interact with a valid target through one portal;
- a nearer ordinary wall hit beats a farther portal intersection;
- a nearer valid portal intersection beats a farther ordinary wall hit;
- query can traverse A->B and a bounded recursive pair view without infinite loop;
- remaining-distance accounting is correct across hops;
- sweep retains expected radius/shape through transform.

---

### P11 — Support policy / endpoint lifecycle

**Goal:** make endpoint ownership explicit and lifecycle cleanup deterministic.

#### V1 support policy

For the initial Core Portal Fidelity Seal, **portal support primitives are static-only unless a separate moving-support feature is explicitly authorized**.

Placement validation must reject unsupported movable support rather than accidentally allowing it.

Required static-support lifecycle work:

- cleanup all movement ignores/constraints/proxies on endpoint invalidation;
- cancel or reject portal replacement while a traveller is in an unsafe crossing state;
- reset render, slice, query and hysteresis state atomically;
- destroyed support invalidates its endpoint safely.

Deferred moving-support work, if later authorized:

- store portal frame relative to its support;
- update endpoint world transform before render/traversal/physics queries;
- move render plane, clip plane and aperture gate atomically;
- define migration/rejection behavior for travellers already crossing a moving endpoint.

**Why this change is better:** the current project does not need moving portal surfaces to validate the core mechanic. Making static support an explicit product rule removes accidental undefined behavior without forcing a separate moving-frame physics problem into the first seal.

---

### P12 — Recursion, performance and temporal quality

**Goal:** keep the final visual system stable at practical performance cost.

Requirements:

- finite recursion depth with a hard maximum;
- view/frustum visibility test before capture;
- adaptive render-target scale based on portal screen area where useful;
- no per-frame render-target reallocation when dimensions are unchanged;
- deepest-first recursion remains deterministic;
- profile GPU cost of two portals at recursion depths 1/2/3/4;
- profile Lumen SceneCapture cost;
- if P1 native clip planes require a project-wide renderer setting, measure its cost in the same performance pass;
- optional update throttling is allowed only when it is visually indistinguishable for the target use case;
- no optimization may reintroduce one-frame stale portal images during movement/crossing.

Target performance budget should be recorded after profiling on the project's target development GPU rather than invented in this document.

---

## 8. Rendering defect fix — concrete first implementation change

The first code change after this plan should be narrowly scoped to the current visual artifact.

### Current production path

```text
Player projection
    -> custom reversed-Z ObliqueProjection(exit plane)
    -> SceneCapture custom projection
    -> render target
    -> ScreenPosition.ViewportUV portal material
```

### First candidate change

```text
Player projection (unchanged spatial projection)
    + SceneCapture native clip plane at exit
    -> render target
    -> same portal material
```

The change should intentionally leave traversal and physics untouched so any visual difference can be attributed to clipping.

Diagnostic comparison toggle during development:

```text
Portal.RenderClipMode = NativeClipPlane | ObliqueFallback
```

This toggle is for validation and rollback only; final production should have one selected preferred path.

Required new tests:

- camera 1-5 cm from aperture;
- clip plane nearly parallel to view direction;
- clip plane nearly perpendicular to view direction;
- exit support with finite thickness;
- portal at wall/floor/ceiling orientations;
- recursion depth >= 2 under the same cases;
- fallback denominator near the configured validity threshold;
- invalid fallback math never silently produces an unclipped frame.

---

## 9. Material and compositing rules

The portal surface material remains responsible for aperture/rim presentation, not spatial math.

Rules:

- use screen/viewport UV only for sampling the capture that was rendered using the matching player projection;
- do not distort the interior image to hide projection errors;
- apply exposure exactly once;
- keep the oval aperture mask and animated rim independent from the captured scene;
- clipping of crossing travellers occurs in traveller materials/proxies, not by cutting arbitrary destination geometry in the portal surface material;
- support all production material slots needed by portal travellers.

A material workaround is not an acceptable fix for a wrong capture camera or wrong clip plane.

---

## 10. State and ownership rules

Portal state must have one owner.

`AInteriorPortalSystem` remains the map-level owner of:

```text
blue/orange endpoint pairing
placement/clear lifecycle
portal query routing
traveller registry
render orchestration
```

The camera/controller path owns only transient mapped view orientation/recovery state.

Each traveller state owns only transient traversal/proxy data for that traveller.

Temporary state that must always be paired with cleanup:

```text
support collision ignores
free collision-suppression constraints
visual proxies
remote physics proxies
slice material parameters
held-through-portal state
last-exit hysteresis
portal camera orientation state
```

Reset, endpoint replacement, actor destruction and EndPlay must all leave the ordinary world collision/camera configuration restored.

---

## 11. Failure and fallback strategy

### Rendering

If native clip plane causes a specific unsupported SceneCapture/Lumen regression or unacceptable measured project-wide cost, keep the stabilized oblique implementation available until a better native path passes the complete gate. Do not silently revert to the currently unstable matrix.

### Camera

If arbitrary roll/horizon behavior conflicts with the existing first-person movement model, preserve mathematically correct crossing orientation through transfer/clearance first, then apply a documented comfort recovery. Do not clamp Euler angles on the transfer frame to hide the conflict.

### Collision

If aperture-local support filtering cannot be made safe for high-speed rigid bodies at game-thread frequency, require a substep/contact-level solution for those bodies or reduce the supported traveller set explicitly. Do not claim high-speed full fidelity from Tick-only broad wall ignores.

### Physics

If P8A cannot prove stable contact mapping:

- retain correct full-body traversal and visual slicing;
- disable remote proxy collision;
- seal Core Portal Fidelity if all core gates pass;
- document Full Physics Fidelity as deferred.

### Constraints

If cross-portal constraints cannot be made stable, reject unsupported constrained traversal explicitly rather than allowing undefined behavior.

### Performance

Reduce recursion depth/resolution before changing transform/camera correctness. Spatial correctness has priority over decorative recursion depth.

---

## 12. Automated validation plan

Extend `SlayTheSpireDemo.Interior.Portals` focused Automation with the following groups.

### Math

- A->B->A position round trip;
- quaternion round trip;
- linear/angular velocity magnitude preservation;
- arbitrary portal orientations;
- aperture boundary/margin tests;
- high-speed swept crossing.

### Rendering math

For the fallback oblique path if retained:

- reversed-Z near-boundary correctness;
- grazing-angle denominator guard;
- invalid denominator handling never produces NaN or silently unclipped output;
- consistent front/back retained half-space.

For native clip path:

- test computed clip base/normal orientation;
- test portal/support relative offset selection independent of rendering.

### Camera state

- crossing quaternion maps exactly through arbitrary portal orientation;
- transfer-frame quaternion is not modified by horizon recovery;
- recovery begins only after declared clearance state;
- reset/portal clear removes transient camera state.

### Placement

- incomplete rim rejected;
- blocked opening rejected;
- overlapping portal rejected;
- unapproved surface rejected;
- floor/wall/ceiling frames stable;
- movable support rejected while V1 static-only support policy is active.

### Traveller state / fit

- register/unregister;
- destroy during approach;
- reset during ordinary idle state;
- cleanup of ignore/constraint/proxy state;
- no immediate re-entry after transfer;
- sphere/capsule/box fit boundary tests;
- long-thin box orientation pass/fail cases;
- compound primitive conservative rejection.

### Aperture collision gate

- swept entry into legal prism;
- swept lateral exit restores collision;
- high-speed diagonal motion cannot bypass solid rim/support;
- endpoint reset restores all ignored collision.

### Portal query

- nearest ordinary world hit beats farther portal;
- nearest portal beats farther ordinary world hit;
- one-hop line trace;
- multi-hop bounded trace;
- remaining-distance accounting;
- cycle protection;
- no-collision decorative portal mesh is not required for query routing;
- sweep shape preserved.

### P8A physics spike

Where automation can observe deterministic state:

- one remote contact maps one impulse;
- mapped impulse direction round trip;
- mapped contact point produces angular response;
- proxy cleanup after clear/destroy.

Automation cannot prove final visual continuity or Chaos contact feel. Those remain manual PIE gates.

---

## 13. Manual PIE acceptance matrix

A release candidate must pass all applicable cells below in both directions unless noted.

| Seal | Area | Scenario | Expected result |
|---|---|---|---|
| Core | Visual | far centered view | destination aligned, no wall leak |
| Core | Visual | near centered view | no near-plane flash |
| Core | Visual | grazing side view | no giant triangle / diagonal wedge |
| Core | Visual | move around aperture | correct parallax |
| Core | Visual | cross camera plane slowly | no image/roll jump |
| Core | Visual | recursion facing pair | stable finite recursion |
| Core | Visual | bright->dark / dark->bright | no double exposure/gamma discontinuity |
| Core | Player | walk wall->wall | seamless transfer |
| Core | Player | sprint/jump | momentum preserved |
| Core | Player | fall floor->wall | exit velocity correctly rotated |
| Core | Player | blocked exit | no embed/teleport |
| Core | Player | edge of aperture | cannot escape through rim/wall |
| Core | Player | high-speed diagonal aperture approach | cannot leak through support beside aperture |
| Core | Physics | throw cube | speed preserved |
| Core | Physics | spinning cube | angular axis transformed |
| Core | Physics | high-speed cube | no missed plane |
| Core | Physics | long thin body fit | only fitting orientation passes |
| Core | Physics | partial insertion | visual split continuous |
| Core | Grab | carry through portal | no handle explosion, angular state stable |
| Core | Grab | object remains opposite side | target remains coherent |
| Core | Query | portal nearer than world hit | query continues through portal |
| Core | Query | world hit nearer than portal | world hit returned |
| Core | Query | interact through portal | correct remote hit |
| Core | Lifecycle | clear/re-place after use | no stale image/proxy/ignore/camera state |
| Core | Existing map | E/Q/F/M and ordinary movement | no regression |
| Full Physics | Physics | remote-half wall contact | mapped response before authority swap |
| Full Physics | Physics | remote movable-body contact | plausible impulse transfer |
| Full Physics | Physics | oblique/friction contact | stable linear + angular/tangential response |
| Full Physics | Physics | repeated authority swap | no energy growth/proxy leak |
| Full Physics | Constraint | supported constrained traversal | matches documented policy |

Evidence should record both the action and observation. A C++ pass alone must never be used as evidence for rows that are inherently visual/physical.

---

## 14. Recommended implementation order

The execution order intentionally differs from the feature numbering:

```text
P0 reproduce/instrument
-> P1 stable clip-plane candidate + production-path gate
-> P2 render/exposure fidelity
-> P3 quaternion camera/player crossing
-> P4 swept aperture-local collision gating
-> P5 generic traveller + geometry-aware conservative fit
-> P10 portal-aware query layer
-> P6 full-body physics + held-object hardening using PortalQuery
-> P7 production visual slicing
-> P11 static-support lifecycle hardening
-> P12 core recursion/performance profiling
-> CORE PORTAL FIDELITY SEAL
-> P8A dual-space contact feasibility spike
-> P8B full dual-space contact bridge (only if P8A passes)
-> P9 supported constraint policy/bridge
-> P12 final full-physics profiling as affected
-> FULL PHYSICS FIDELITY SEAL
```

Why P10 moves earlier in execution: Physics Handle and later interaction features already need portal-aware targeting. Building the shared query layer before P6 prevents more bespoke portal trace rules from accumulating and then being migrated later.

Why P8 moves after the core seal: the current visible rendering/crossing/collision/query problems are independently solvable and more important to a usable portal feature. The Chaos dual-space bridge should not block acceptance of a stable core system.

---

## 15. Suggested commit / delivery boundaries

Keep changes reviewable and reversible.

Recommended boundaries:

```text
portal-render-clip-fix
portal-render-fidelity
portal-camera-traversal-state
portal-collision-aperture-gate
portal-traveller-registry-shape-fit
portal-query-layer
portal-physics-transfer-handle-hardening
portal-visual-slice-generalization
portal-static-support-lifecycle
portal-core-performance
portal-core-fidelity-seal
portal-dual-space-contact-spike
portal-dual-space-contact-bridge
portal-constraint-policy
portal-full-physics-fidelity-seal
```

Each boundary should update focused tests and the relevant current-status documentation. Do not mix unrelated battle/UI refactors into these commits.

---

## 16. Definition of Done

### 16.1 Core Portal Fidelity Complete / Validated / Sealed

The project may mark **CORE PORTAL FIDELITY COMPLETE / VALIDATED / SEALED** only when:

1. the current near/grazing visual artifact is gone;
2. the selected production clip path has passed correctness and measured performance review;
3. camera projection and clipping pass the complete visual matrix;
4. transfer-frame camera orientation remains quaternion-correct through arbitrary supported portal orientations and any horizon recovery begins only after clearance;
5. player traversal is continuous for supported portal orientations;
6. ordinary rigid-body position/orientation/linear/angular momentum transfer passes;
7. geometry-aware conservative aperture fit is used for supported traveller shapes;
8. aperture-local collision suppression uses continuous/swept or physics-step-safe gating appropriate to the supported traveller speed;
9. partial crossing is visually continuous;
10. Physics Handle crossing is stable and preserves required linear/angular state;
11. portal-aware traces/sweeps are available to exploration interactions and analytically compete with ordinary world hits;
12. V1 support policy is explicit and lifecycle cleanup is deterministic;
13. recursion is finite, stable and profiled;
14. focused Automation passes;
15. required Core manual PIE gates pass in both directions;
16. existing exploration controls/interaction behavior show no regression;
17. `docs/InteriorPortals.md`, validation evidence and any active checkpoint agree with the sealed state.

### 16.2 Full Physics Fidelity Complete / Validated / Sealed

The project may additionally mark **FULL PHYSICS FIDELITY COMPLETE / VALIDATED / SEALED** only when:

1. Core Portal Fidelity is already sealed;
2. P8A proved stable contact acquisition and mapped impulse application;
3. remote-half destination collision/contact is implemented and validated;
4. mapped contacts produce stable linear and angular response without duplicate energy injection;
5. friction/tangential behavior is acceptable for the documented supported cases;
6. authority swap does not create energy explosions or proxy leaks;
7. the supported multi-body constraint policy is implemented, or unsupported cases are explicitly outside the claimed Full Physics scope;
8. required Full Physics manual PIE gates pass;
9. performance is profiled after the dual-space bridge is enabled.

Until the Core seal passes, the correct project status is **implemented baseline / not core-fidelity accepted**.

After the Core seal but before the Full Physics seal, the correct status is **core portal fidelity sealed / full physics fidelity deferred or in progress**.
