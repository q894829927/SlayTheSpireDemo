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

Current review baseline: `94c2d9d568dae50d1d4455cb188e194713ee5da6` (`main`, 2026-09-12).

This initiative is independent from the card-battle phase plan. It must not change battle architecture, gameplay authority, UI phase state or retained Legacy UI policy.

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

## 2. Full-fidelity acceptance definition

The implementation is not accepted merely because teleportation works. It is accepted only when the following observable contract holds.

### 2.1 Visual continuity

- Looking through either portal shows the paired portal's space from the correct transformed eye position and orientation.
- Parallax remains correct while moving laterally, vertically, toward, away from and across the aperture.
- Portal contents remain aligned with the physical aperture at center view, edge view and grazing angles.
- Geometry behind the exit support plane never leaks into the portal image.
- No giant triangles, diagonal wall wedges, near-plane flashes, one-frame black frames or stale render targets are visible.
- The transition from looking through a portal to physically crossing it has no visible camera jump.
- Recursive portal views remain spatially coherent up to the configured finite recursion depth.
- Portal rendering applies exposure/tone mapping exactly once and does not visibly diverge from the destination room solely because it is seen through a portal.
- Local portal rim art is allowed to be stylized, but the scene inside the aperture must remain spatially faithful.

### 2.2 Player traversal continuity

- Crossing works wall-to-wall, wall-to-floor, floor-to-wall, floor-to-ceiling and arbitrary valid portal orientations.
- Position, camera orientation and linear momentum map through the same rigid portal transform.
- Entry speed magnitude is preserved except for ordinary game movement rules applied after exit.
- High-speed crossings cannot tunnel past the portal plane.
- The player cannot cross if the full capsule does not fit the aperture or the exit pose is blocked.
- The player cannot escape through the supporting wall outside the legal portal opening.
- Crossing cannot immediately retrigger the exit portal in a ping-pong loop.

### 2.3 Rigid-body continuity

- Position, body orientation, linear velocity and angular velocity map correctly.
- Momentum magnitude is unchanged by equal-scale portals.
- A rotating object exits with the correctly transformed rotation axis.
- High-speed rigid bodies cannot miss the crossing.
- A rigid body that does not fit the aperture cannot pass.
- A partially inserted object is visually split between spaces without duplicate visible geometry.
- Final full-fidelity physics includes a remote collision representation so the portion visible through the remote portal can participate in destination-side contacts before center-plane authority transfer.
- Crossing while held by a Physics Handle remains stable and cannot produce an impulse explosion.

### 2.4 Interaction continuity

Portal-aware world queries must support at least:

```text
interaction traces
Physics Handle targeting
weapon / projectile traces used by this exploration mode
portal placement queries where intentionally allowed
future laser/beam-style queries without rewriting portal math
```

A query that enters one portal can continue from the paired portal with transformed origin/direction and a finite recursion limit.

### 2.5 Lifecycle and robustness

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

### 3.6 Held-object support — PARTIALLY IMPLEMENTED

The system already has a `UPhysicsHandleComponent` path that:

- transforms held target location through the portal;
- transforms target rotation;
- ignores the relevant support wall during cross-portal targeting;
- prevents the handle target from treating the two spaces as ordinary distant world points.

This path still requires dedicated angular-velocity preservation and crossing-state hardening described later in this plan.

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

PortalTraveller state
    previous pose / crossing state / support gating / proxy state

PortalQuery
    recursive line/sweep/interaction traversal

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

The work is intentionally staged so each stage has a visible acceptance gate and the most severe current defect is addressed first.

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

Preferred implementation:

1. enable UE's project support for global clip planes required by SceneCapture clip-plane rendering;
2. use `SceneCaptureComponent2D::bEnableClipPlane`;
3. set:

```cpp
ClipPlaneBase   = ExitPortalLocation + ExitForward * ClipBias;
ClipPlaneNormal = ExitForward;
```

4. retain the player's projection matrix for matching FOV/aspect, but remove oblique near-plane mutation from the normal production path;
5. disable the redundant `bOverride_CustomNearClippingPlane` path unless an independently justified camera near clip is required;
6. keep the exit portal actor hidden from its own capture;
7. do **not** permanently hide the entire support wall as the final solution, because doing so can create a visually oversized hole beyond the aperture.

`ClipBias` must be data-driven/tunable and chosen from portal/support geometry, not hard-coded as a magic large distance. Start with a minimal centimeter-scale bias and validate at all view angles.

Fallback only if native SceneCapture clip planes prove incompatible with required Lumen/render behavior:

- keep the custom oblique path behind one explicit implementation switch;
- normalize/orient the view plane consistently;
- reject or clamp ill-conditioned denominators using a meaningful epsilon rather than `UE_SMALL_NUMBER` alone;
- preserve the previous valid matrix rather than emitting a pathological matrix for one frame;
- add extreme near/grazing unit tests;
- remove the second competing near-clipping mechanism.

Acceptance:

- no support-wall leak in the full P0 view matrix;
- no diagonal giant triangle at grazing angles;
- no one-frame flash when camera approaches or crosses the portal plane;
- projection alignment remains unchanged.

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

Required work:

- make camera-view mapping and physical traversal use the exact same crossing frame;
- retain continuous previous/current eye segment detection;
- add explicit crossing state:

```text
Outside
ApproachingEntry
IntersectingAperture
Transferred
ClearingExit
```

- use hysteresis to avoid plane-chatter around `X ~= 0`;
- preserve transformed velocity before/after `TeleportPhysics`;
- preserve full control-view quaternion through arbitrary portal orientations;
- keep Character actor-root orientation policy explicit:
  - upright capsule/world-gravity behavior remains controlled by CharacterMovement;
  - camera orientation may carry transformed pitch/yaw/roll as required by the portal view;
  - any post-exit horizon correction must be an intentional comfort policy, not an accidental Euler clamp;
- make exit blocking use the actual target capsule pose;
- make exit offset minimal and derived from collision clearance.

Acceptance scenarios:

- walk through slowly;
- sprint through;
- jump/fall through;
- floor->wall launch;
- wall->floor fall;
- ceiling-related orientation where placement is allowed;
- enter while looking sharply up/down/sideways;
- repeatedly alternate portals without camera snap or retrigger.

---

### P4 — Aperture-local collision passage instead of broad wall ignore

**Goal:** allow traversal through the portal hole while preventing escape through the rest of the support surface.

Current behavior temporarily ignores the whole support primitive when the character/body is close enough and fits the aperture. This is safe as a prototype but not full-fidelity collision.

Target behavior:

- support collision is bypassed only while the traveller occupies the legal portal prism and is committed to passage;
- leaving the aperture laterally immediately restores normal support collision;
- restore uses a safe non-penetrating pose or depenetration path;
- cleanup on portal clear/destroy always restores collision.

Implementation options, in preferred order:

1. aperture-gated collision state with per-frame/substep validation and immediate restore;
2. segmented/custom support collision that exposes an actual portal-shaped opening when the asset topology permits;
3. low-level Chaos contact modification only if higher-level solutions cannot provide robust aperture-local filtering.

Do not globally disable shared wall collision.

Acceptance:

- player/body can pass through the portal center;
- the same traveller cannot move sideways through the support wall beside the portal while passage gating is active;
- closing/moving a portal cannot leave a permanent collision hole.

---

### P5 — Generic traveller model and exact body fit

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

Exact-fit strategy:

- inspect body collision geometry (box/sphere/capsule/convex as available);
- transform representative/support points into portal local space;
- project the oriented shape against the elliptical aperture;
- reject shapes that would contact the rim;
- keep a small configurable safety margin.

Acceptance:

- long thin object can pass in an orientation that truly fits;
- the same object is rejected in an orientation that does not fit;
- sphere/cube behavior does not regress.

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
- verify destination overlap before authority transfer where required;
- protect against repeated transfer in the same simulation interval.

Held-object requirements:

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

---

### P8 — Dual-space collision/contact bridge

**Goal:** make the remote half of a partially crossed rigid body physically interact with the destination world before the authoritative body center transfers.

This is the largest remaining physics feature and must be implemented only after P5-P7 are stable.

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

Required behavior:

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

1. begin with a kinematic/contact-reporting remote proxy and explicit impulse mapping;
2. if UE-level hit callbacks cannot provide sufficient contact/friction fidelity, introduce a narrowly scoped Chaos contact bridge inside the project;
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

If full cross-portal constraint bridging is too risky for the initial seal, it must be an explicit deferred limitation with placement/traversal rejection rather than undefined physics.

---

### P10 — Portal-aware world-query layer

**Goal:** make a portal a real spatial connection for traces and interaction, not only for cameras and bodies.

Provide a reusable bounded query function equivalent to:

```cpp
PortalLineTrace(..., MaxPortalDepth)
PortalSweep(..., MaxPortalDepth)
```

Algorithm:

```text
trace in current space
    -> ordinary hit: return
    -> portal aperture hit:
         consume travelled distance
         map hit point + direction through pair
         apply tiny forward epsilon
         continue in exit space
         decrement recursion budget
```

Requirements:

- prevent immediate self-rehit at the exit plane;
- finite recursion/cycle protection;
- preserve ignored actors/components correctly across each segment;
- return a segmented debug path for diagnostics;
- support sweep radius/shape where Physics Handle targeting needs volume rather than a ray.

Adopt it for:

- E interaction where appropriate;
- Physics Handle targeting;
- exploration weapon/projectile/beam traces where present;
- any later laser puzzle mechanic.

Portal placement shots may opt in or out as an explicit gameplay rule; they must not accidentally inherit behavior from an unrelated trace implementation.

Acceptance:

- interact with a valid target through one portal;
- query can traverse A->B and a bounded recursive pair view without infinite loop;
- sweep retains expected radius through transform.

---

### P11 — Moving support / endpoint lifecycle

**Goal:** make portal placement robust if a valid support primitive moves or rotates.

Required work:

- store portal frame relative to its support where dynamic support is permitted;
- update endpoint world transform before render/traversal/physics queries;
- move render plane, clip plane and aperture gate atomically;
- forbid moving a portal while a traveller is in an unsafe ambiguous crossing state unless the traveller state can be migrated safely;
- cleanup all ignores/constraints/proxies on endpoint invalidation.

If this project chooses static-only portal supports, that restriction must be explicit in placement validation rather than accidental.

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

### Target first change

```text
Player projection (unchanged spatial projection)
    + SceneCapture native clip plane at exit
    -> render target
    -> same portal material
```

The change should intentionally leave traversal and physics untouched so any visual difference can be attributed to clipping.

Diagnostic fallback toggle during development:

```text
Portal.RenderClipMode = NativeClipPlane | ObliqueFallback
```

This toggle is for validation and rollback only; final production should have one preferred path.

Required new tests:

- camera 1-5 cm from aperture;
- clip plane nearly parallel to view direction;
- clip plane nearly perpendicular to view direction;
- exit support with finite thickness;
- portal at wall/floor/ceiling orientations;
- recursion depth >= 2 under the same cases.

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
```

Reset, endpoint replacement, actor destruction and EndPlay must all leave the ordinary world collision configuration restored.

---

## 11. Failure and fallback strategy

### Rendering

If native clip plane causes a specific unsupported SceneCapture/Lumen regression, keep the stabilized oblique implementation available until native clipping passes the full manual gate. Do not silently revert to the currently unstable matrix.

### Physics

If dual-space contact bridging is not stable enough for a production seal:

- retain correct full-body traversal and visual slicing;
- disable remote proxy collision;
- document the limitation explicitly;
- do not claim full-fidelity physics acceptance.

### Constraints

If cross-portal constraints cannot be made stable, reject constrained traversal explicitly rather than allowing undefined behavior.

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
- no invalid/NaN matrices;
- consistent front/back retained half-space.

For native clip path:

- test computed clip base/normal orientation;
- test portal/support relative offset selection independent of rendering.

### Placement

- incomplete rim rejected;
- blocked opening rejected;
- overlapping portal rejected;
- unapproved surface rejected;
- floor/wall/ceiling frames stable;
- dynamic-support restriction enforced if not supported.

### Traveller state

- register/unregister;
- destroy during approach;
- reset during ordinary idle state;
- cleanup of ignore/constraint/proxy state;
- no immediate re-entry after transfer.

### Portal query

- one-hop line trace;
- multi-hop bounded trace;
- remaining-distance accounting;
- cycle protection;
- sweep shape preserved.

Automation cannot prove final visual continuity or Chaos contact feel. Those remain manual PIE gates.

---

## 13. Manual PIE acceptance matrix

A release candidate must pass all applicable cells below in both directions unless noted.

| Area | Scenario | Expected result |
|---|---|---|
| Visual | far centered view | destination aligned, no wall leak |
| Visual | near centered view | no near-plane flash |
| Visual | grazing side view | no giant triangle / diagonal wedge |
| Visual | move around aperture | correct parallax |
| Visual | cross camera plane slowly | no image jump |
| Visual | recursion facing pair | stable finite recursion |
| Visual | bright->dark / dark->bright | no double exposure/gamma discontinuity |
| Player | walk wall->wall | seamless transfer |
| Player | sprint/jump | momentum preserved |
| Player | fall floor->wall | exit velocity correctly rotated |
| Player | blocked exit | no embed/teleport |
| Player | edge of aperture | cannot escape through rim/wall |
| Physics | throw cube | speed preserved |
| Physics | spinning cube | angular axis transformed |
| Physics | high-speed cube | no missed plane |
| Physics | partial insertion | visual split continuous |
| Physics | remote-half wall contact | mapped response before authority swap |
| Physics | remote movable-body contact | plausible impulse transfer |
| Grab | carry through portal | no handle explosion |
| Grab | object remains opposite side | target remains coherent |
| Query | interact through portal | correct remote hit |
| Lifecycle | clear/re-place after use | no stale image/proxy/ignore |
| Existing map | E/Q/F/M and ordinary movement | no regression |

Evidence should record both the action and observation. A C++ pass alone must never be used as evidence for the rows above that are inherently visual/physical.

---

## 14. Recommended implementation order

The execution order is intentionally different from a feature-list order:

```text
P0 reproduce/instrument
-> P1 stable clip plane
-> P2 render/exposure fidelity
-> P3 camera/player crossing
-> P4 aperture-local collision gating
-> P5 generic traveller + exact fit
-> P6 full-body physics + held object hardening
-> P7 production visual slicing
-> P8 dual-space contact bridge
-> P9 constraints policy/bridge
-> P10 portal-aware queries
-> P11 moving-support lifecycle
-> P12 recursion/performance polish
-> final full acceptance
```

Do not start the dual-space physics bridge while the current SceneCapture clipping artifact remains unresolved. The rendering bug is visible, reproducible and isolated enough to fix first.

---

## 15. Suggested commit / delivery boundaries

Keep changes reviewable and reversible.

Recommended boundaries:

```text
portal-render-clip-fix
portal-render-fidelity
portal-traversal-state
portal-collision-aperture-gate
portal-traveller-registry-shape-fit
portal-physics-transfer-hardening
portal-visual-slice-generalization
portal-dual-space-contact-bridge
portal-query-layer
portal-lifecycle-performance
portal-full-fidelity-seal
```

Each boundary should update focused tests and the relevant current-status documentation. Do not mix unrelated battle/UI refactors into these commits.

---

## 16. Definition of Done

The portal initiative can be marked **FULL-FIDELITY COMPLETE / VALIDATED / SEALED** only when:

1. the current near/grazing visual artifact is gone;
2. camera projection and clipping pass the complete visual matrix;
3. player traversal is continuous for supported portal orientations;
4. ordinary rigid-body position/orientation/linear/angular momentum transfer passes;
5. partial crossing is visually continuous;
6. remote-half destination collision/contact is implemented and validated if claiming full physics fidelity;
7. Physics Handle crossing is stable;
8. portal-aware traces/sweeps are available to exploration interactions;
9. collision suppression is aperture-local and cleans up deterministically;
10. recursion is finite, stable and profiled;
11. focused Automation passes;
12. required manual PIE gates pass in both directions;
13. existing exploration controls/interaction behavior show no regression;
14. `docs/InteriorPortals.md`, validation evidence and any active checkpoint agree with the sealed state.

Until all relevant items above pass, the correct project status is **implemented baseline / not full-fidelity accepted**.
