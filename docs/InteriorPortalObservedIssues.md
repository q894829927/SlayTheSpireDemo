# Interior Portal — Observed Issues and Regression Targets

Date: **2026-09-13**

Status:

```text
ACTIVE DEFECT LOG /
USED TO DRIVE THE FULL-FIDELITY PORTAL IMPLEMENTATION /
DO NOT MARK CORE PORTAL FIDELITY SEALED WHILE A BLOCKING ITEM BELOW REMAINS OPEN
```

Related authority:

- `docs/InteriorPortalFullFidelityImplementationPlan.md`
- `docs/InteriorPortals.md`
- `Source/SlayTheSpireDemo/Interior/InteriorPortalSystem.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorPortal.h/.cpp`
- `Source/SlayTheSpireDemo/Interior/InteriorChildCharacter.h/.cpp`
- `tools/setup_interior_portals.py`

This document records defects observed in live PIE validation while implementing the full-fidelity portal plan. It is not a substitute for the implementation plan; it is the concrete regression list that each delivery stage must close.

---

## 1. P1 clipping defect — black support wall / giant diagonal triangle

### Observed symptom

At near or oblique portal viewing angles, the portal image could show either:

- a large black wall region; or
- a giant diagonal triangular/wedge-shaped piece of geometry.

### Current status

**Mitigated in the tested normal views, not fully sealed.**

The current branch separates the logical portal plane from the cosmetic visual surface and uses the native SceneCapture clip plane as the P1 production candidate. The originally reproduced black-wall / giant-triangle defect is no longer visible in the supplied normal PIE screenshots.

### Why it happened

The previous path relied on a custom oblique near-plane projection with a very small offset from the exit support surface. At near/grazing configurations, the support wall could intersect or leak across the custom clipping plane, and an ill-conditioned projection could produce extreme triangle distortion.

### Remaining regression matrix

P1 is not sealed until all of the following pass:

```text
centered far
centered near
left/right aperture edge
high/low aperture edge
grazing angle
camera crossing the plane
recursion depth >= 2
rapid portal replacement / clear
```

---

## 2. P2 exposure-domain mismatch — portal view too dark or over-corrected

### Observed symptom

The same destination scene has substantially different brightness when viewed:

```text
through the portal
vs
standing in the destination directly
```

Observed A/B results:

- raw portal RenderTarget path (`PortalViewExposureCorrection = 0`) is visibly too dark;
- the first `EyeAdaptationInverse` correction path (`PortalViewExposureCorrection = 1`) is visibly too bright / over-corrected;
- when eye adaptation is disabled and pre-exposure is forced to `1.0`, the direct and portal brightness broadly converge, although the whole bright scene becomes overexposed.

### Current status

**Open — blocking P2 visual fidelity.**

### Why the first correction is insufficient

The SceneCapture uses `SCS_SceneColorHDRNoAlpha`, so the RenderTarget participates in SceneColor / pre-exposure behavior. Applying the player's `EyeAdaptationInverse` to that texture does not necessarily invert the SceneCapture's own pre-exposure state. The current 0/1 result demonstrates that a constant gain or a full inverse-eye-adaptation transform is not a correct general solution.

### Required direction

The next P2 correction must explicitly reason about the **capture view's own pre-exposure**, not tune a scene-specific brightness multiplier.

Target pipeline:

```text
SceneCapture SceneColor
    -> remove/normalize Capture PreExposure
    -> scene-linear portal value
    -> emit through portal material
    -> player view applies its normal exposure/tonemap once
```

### Acceptance

Across both bright->dark and dark->bright portal directions:

- no obvious black crush;
- no white blowout caused by the portal material;
- no large brightness jump when the player physically crosses the portal;
- auto-exposure adaptation does not make the portal drift independently from the destination view.

---

## 3. Partial-crossing player/held-item visual discontinuity

### Observed symptom

When the player is only partially through a portal, first-person geometry such as the flashlight can disappear at the portal plane. The object is visible on the source side but the portion that should already exist on the destination side is missing.

### Current status

**Open — blocking seamless crossing.**

### Why it happens

The current partial-crossing proxy/slice implementation exists for configured rigid-body `PhysicsTravellers`, but the player and first-person attached visuals do not yet have an equivalent remote visual representation.

The current visual model is effectively:

```text
source first-person mesh
        |
        | portal plane
        X destination-side copy does not exist
```

The portal surface correctly occludes geometry behind it, but the SceneCapture cannot show a destination-side copy that was never created.

### Required behavior

Player-owned visuals that can intersect the aperture must participate in the same portal visual contract as rigid-body travellers:

```text
source visual
    + source-side slice
    + mapped remote visual proxy
    + destination-side inverse slice
    = one continuous object across the portal
```

The implementation must define which player-owned components participate, at minimum the current first-person flashlight body/rim/lens/grip and any future held-item visual registered as a PortalTraveller visual child.

### Acceptance

- slow partial insertion does not make the flashlight/held item vanish;
- source and remote halves meet continuously at the logical portal plane;
- no duplicate full object is visible;
- the proxy is removed/authority-swapped deterministically after crossing clears;
- rapid forward/backward motion does not leave a stale proxy.

---

## 4. Flashlight illumination is not portal-aware

### Observed symptom

When the player shines the flashlight at/through the portal, illumination can appear to originate from behind the supporting wall rather than continuing cleanly through the aperture.

### Current status

**Open — visual/gameplay lighting defect.**

### Current implementation constraint

The flashlight is an ordinary movable `USpotLightComponent` attached to the first-person `FlashlightRig`. It follows the player/camera transform. There is currently no portal-specific light transport, no destination-side flashlight proxy, and no aperture mask for the light cone.

This means the existing light behaves as an ordinary world-space spotlight even while the player's visual/camera state is transitioning between two portal spaces.

### Why this matters even though full physical light transport is not a V1 goal

The full implementation plan currently excludes physically simulated general light transport through portals. That non-goal should remain for arbitrary world lights, GI and physically exact radiance transport.

However, the player's flashlight is a direct gameplay-facing attached tool. Its visible beam must obey portal-space continuity or it visibly contradicts the portal traversal. Therefore this issue is narrower than general portal light transport and should be treated as a **supported first-person traveller effect**, not as a promise to simulate arbitrary lights through portals.

### Required behavior

Preferred staged solution:

```text
source flashlight
    -> determine whether the cone intersects the legal portal aperture
    -> map source location + direction through Entry->Exit
    -> enable/update a destination-side remote SpotLight proxy
    -> prevent the source light from visually leaking through the support wall
    -> constrain remote illumination to light that legitimately passes the aperture
```

The first implementation may use a mapped remote spotlight proxy plus an aperture/light-function restriction. A later renderer-specific solution may improve the aperture clipping if necessary.

### Acceptance

- shining at ordinary wall area outside the aperture never lights the remote side;
- shining through the aperture lights the destination side from the mapped direction;
- the beam does not appear to originate from behind the support wall;
- crossing while the flashlight is on does not cause one-frame double lighting or a light pop;
- turning the flashlight off removes both authoritative and remote lighting immediately.

---

## 5. Critical traversal defect — rapid enter/exit can place the player behind the support wall

### Observed symptom

Repeatedly moving back and forth through a portal can, with non-deterministic-looking timing, leave the player on the wrong side of the supporting wall / behind the wall rather than cleanly inside one of the two valid portal spaces.

### Severity

**Critical gameplay correctness bug.**

This defect takes priority over additional visual polish because it allows the player to escape the intended collision topology.

### Current contributing behavior

The current traversal prototype temporarily ignores the entire support primitive when the capsule is near the portal and appears to fit the aperture. The exit-side path also keeps support ignore active while `LastPlayerExit` considers the player to still be clearing the exit.

Conceptually, the current behavior is too close to:

```text
near valid aperture
    -> ignore entire support wall
    -> CharacterMovement runs
    -> restore later
```

rather than:

```text
inside legal swept aperture prism
    -> bypass only the portal opening
    -> immediately restore wall collision on lateral/invalid escape
```

The transfer path also performs a direct teleport placement rather than making collision correctness depend on a swept post-transfer movement step. Exit-overlap validation intentionally ignores the exit support component, which is necessary to pass through the aperture but increases the importance of a correct aperture-local state machine.

### Why the bug appears probabilistic

The defect depends on frame timing and input reversal:

```text
frame N: transfer commits; exit support is temporarily ignored
frame N+1: player reverses while still considered ClearingExit
frame N+2: CharacterMovement advances while the whole support is still ignored
frame N+3: collision is restored after the capsule may already be on the invalid side
```

Different frame rates, movement speeds and reversal timing therefore make the issue look random even though the underlying state transition is deterministic.

### Required fix

P3/P4 must replace the broad wall-ignore prototype with an explicit traveller crossing state and a swept aperture-local collision gate.

Required player states:

```text
Outside
ApproachingEntry
IntersectingAperture
Transferred
ClearingExit
```

Required rules:

- at most one transfer commit per traveller per simulation interval;
- `ClearingExit` cannot immediately be treated as a new entry crossing;
- exit hysteresis must require real clearance before the portal is eligible again;
- support bypass remains valid only while the swept capsule occupies the legal portal prism;
- lateral escape from the aperture restores support collision immediately;
- any restore must resolve to a safe non-penetrating pose;
- repeated forward/backward input cannot accumulate stale ignore state;
- portal clear/replacement/destruction always restores support collision.

### Acceptance / regression tests

At minimum test:

```text
slow forward -> backward before fully clear
sprint forward -> immediate backward
hold alternating W/S at the plane
jump through -> reverse in air
approach aperture edge -> reverse/lateral strafe
30/60/120+ FPS
low and high CharacterMovement speeds
100 repeated A<->B traversals
```

Expected result:

- player never ends behind the support wall;
- player never exits through wall area outside the aperture;
- no ping-pong transfer loop;
- no permanent support-ignore state;
- no visible depenetration launch/teleport pop after collision is restored.

---

## 6. Implementation priority resulting from these observations

The newly observed defects change the immediate implementation order.

Recommended order from the current branch state:

```text
1. Keep P1 clipping regression coverage active.
2. Fix P3/P4 traversal-state + aperture-local collision safety.
3. Implement player/first-person partial-crossing visual proxy/slicing.
4. Implement portal-aware flashlight remote-light behavior.
5. Return to P2 capture-pre-exposure normalization and finish direct-vs-portal exposure parity.
6. Continue remaining PortalQuery / rigid-body / dual-space physics work from the main plan.
```

Reason for moving traversal safety ahead of the remaining exposure work: an exposure mismatch is visually wrong, but support-wall escape breaks world topology and can invalidate later physics/query validation. The collision state must therefore become trustworthy before more advanced crossing behavior is layered on top.

---

## 7. Definition of closed for this defect log

An issue may be marked closed only when:

1. the implementation exists on the active portal branch;
2. relevant focused Automation tests are added where practical;
3. the corresponding PIE regression scenario passes;
4. no new workaround contradicts `LogicalPortalFrame`, frame-order, or PortalTraveller contracts in the main plan;
5. the result is recorded in the PR / validation notes.

Do not close an issue solely because a single screenshot looks correct.
