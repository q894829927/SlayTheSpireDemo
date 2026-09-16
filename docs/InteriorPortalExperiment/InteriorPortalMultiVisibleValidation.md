# Interior Portal — STEP 1B.14D-MV Multi-Visible Portal Gate

Date: **2026-09-16**

State:

```text
STEP 1B.14D-C SINGLE-VISIBLE PRODUCTION TSR PARITY = PASS
MV-A ENDPOINT-OWNED PRODUCER = IMPLEMENTED
MV-B DUAL-VISIBLE RUNTIME VALIDATION = PASS
MV-C-A STABLE FULL-FIDELITY CONTROL SURFACE = IMPLEMENTED
MV-C-B SYSTEM LIFECYCLE INTEGRATION = NEXT
MV-D VRAM / PERFORMANCE = REQUIRED FOLLOW-UP
```

## Closed user-visible defect

The previously reproduced failure was:

```text
Blue + Orange visible simultaneously
    -> one endpoint rendered normally
    -> the other endpoint exposed the black physical portal surface
```

Source inspection showed that the old single-visible producer stopped on the first visible portal and the compositor owned only one replaceable request. It also had only one persistent TSR ViewState/history and one secondary depth target, so deleting the first-visible `break` would have corrupted temporal ownership.

MV-A introduced independent endpoint ownership instead of sharing that state.

## MV-A implementation

Commit:

```text
1c6b3262f286f88c222c37f702c1b617af10d23c
portal: add endpoint-owned multi-visible TSR spike
```

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalMultiVisibleTSRSpike.cpp
```

Ownership is now:

```text
Endpoint 0 / Blue
    -> persistent TSR ViewState
    -> temporal-validity / camera-cut state
    -> publication generation
    -> exact FColorSample per completed submission
    -> Blue portal color target
    -> own R32F secondary-depth target
    -> own main BeforeDOF composition extension

Endpoint 1 / Orange
    -> persistent TSR ViewState
    -> temporal-validity / camera-cut state
    -> publication generation
    -> exact FColorSample per completed submission
    -> Orange portal color target
    -> own R32F secondary-depth target
    -> own main BeforeDOF composition extension
```

The two endpoint submissions share only the sequential final renderer scratch. Persistent temporal state, exposure metadata, portal color targets and depth transport remain endpoint-owned.

Each extraction extension is additionally tied to its exact endpoint `FSceneViewStateInterface*`, preventing the Blue Tonemap observer from consuming the Orange additional view family and vice versa.

Per endpoint the accepted publication contract remains:

```text
secondary Tonemap
    -> measure exact PreExposure
    -> queue matching color extraction
    -> queue matching depth transport
    -> complete exact FColorSample
    -> publish only that endpoint's completed request
```

A visibility-generation guard rejects stale in-flight publications after an endpoint leaves the visible set.

## MV-B runtime evidence — PASS

The user rebuilt the branch and ran:

```text
r.AntiAliasingMethod 4
portal.FullViewFamilyTSRPrimaryFraction 0.67
portal.MultiVisibleDiagnostics 1
portal.StartMultiVisibleTSRSpike
```

The PIE screenshot shows both simultaneously visible apertures containing their remote scenes. The previous one-black / one-lit failure is no longer present.

The runtime sequence also exercised multiple visibility states before returning to dual visibility:

```text
VisibleCount=1 / VisibleMask=0x01
VisibleCount=1 / VisibleMask=0x02
VisibleCount=0 / VisibleMask=0x00
VisibleCount=2 / VisibleMask=0x03
```

This proves that both endpoint identities can leave and re-enter the visible set independently before the final dual-visible state.

### Decisive dual-visible telemetry

At Tick 600 and Tick 720:

```text
VisibleCount=2
VisibleMask=0x03
SubmittedMask=0x03
PublishedMask=0x03
```

Explicit dump:

```text
PortalMultiVisible Report
VisibleCount=2
VisibleMask=0x03
SubmittedMask=0x03
PublishedMask=0x03

E0:
  Submitted=403
  ExtractFrame=1689
  Cuts=5
  Continuous=398
  PreExposure=0.00198942167
  Completed=403

E1:
  Submitted=362
  ExtractFrame=1689
  Cuts=4
  Continuous=358
  PreExposure=0.00165191176
  Completed=362
```

Both endpoint extraction frames reach the same current render frame, both have finite positive independent PreExposure values, and both have completed submission ids. `PublishedMask=0x03` proves neither endpoint overwrote or suppressed the other endpoint's main-view composition request.

Additional periodic telemetry continued after the dump, including Endpoint 0 reaching Submitted=480 with continuous history still advancing.

Classification:

```text
MV-B = PASS
DUAL VISIBLE ENDPOINT COVERAGE = PASS
PER-ENDPOINT COMPLETED PUBLICATION = PASS
PER-ENDPOINT TEMPORAL OWNERSHIP = PASS
VISIBILITY TRANSITION / RETURN = PASS
ONE-BLACK-PORTAL DISPLAY REGRESSION = CLOSED
```

The original multi-visible display inconsistency is therefore closed at the renderer-correctness level.

## VRAM finding — production blocker, not correctness failure

The accepted dual-visible screenshot also reports:

```text
Video memory has been exhausted (161.059 MB over budget)
Expect extremely poor performance.
```

This is materially worse than the earlier single-visible discovery runs, which were roughly 50–80 MB over budget.

The new producer already shares one final RGBA16F scratch instead of allocating one per endpoint. The remaining increase is therefore expected to come primarily from resources that are intentionally endpoint-owned for correctness, especially persistent TSR/view history plus per-endpoint color/depth surfaces and renderer history.

Do **not** fix this by:

```text
sharing one ViewState between Blue and Orange
sharing one FColorSample
sharing one persistent color target that main composition still needs
reducing exposure correctness
adding a brightness/gamma workaround
```

Those would reintroduce the already-closed ownership failures.

## MV-C-A — stable full-fidelity control surface

Commit:

```text
7ec91507a10cb5e8c67c7d2e6aaaf621dc5132b7
portal: add full-fidelity renderer control surface
```

Source:

```text
Source/SlayTheSpireDemo/Interior/InteriorPortalFullFidelityRendererControl.cpp
```

Stable control commands now front the accepted multi-visible implementation:

```text
portal.StartFullFidelityRenderer
portal.DumpFullFidelityRenderer
portal.StopFullFidelityRenderer
```

These commands deliberately delegate to the already validated endpoint-owned multi-visible TSR implementation rather than duplicating renderer logic. The historical `portal.StartMultiVisibleTSRSpike` command remains available for A/B validation.

This is only the first half of production promotion. The renderer still needs to be tied to the normal `AInteriorPortalSystem` renderer lifecycle rather than requiring an explicit console start.

## MV-C-B — next: system lifecycle integration

Production lifecycle integration must retain:

```text
per-endpoint persistent ViewState
per-endpoint visibility generation
per-endpoint exact FColorSample
completed-request-only publication
main-view-only composition
additional-view-family isolation
shared sequential final scratch where safe
```

After lifecycle integration, run one focused regression:

```text
single Blue visible
single Orange visible
both visible
both -> one -> both
slant / approach-retreat
cross / look back
```

No second exposure investigation is required unless those regressions produce new evidence.

## MV-D — required VRAM / bounded secondary work

Because the accepted dual-visible path is 161.059 MB over the current video-memory budget, performance is no longer optional before declaring the renderer production-ready.

The next optimization direction is **secondary work bounding**, not temporal-state sharing.

Priority order:

```text
1. Measure persistent allocations attributable to each secondary TSR view.
2. Bound secondary render/output allocation to the conservative projected portal region where feasible.
3. Preserve independent endpoint ViewStates while reducing full-frame transient/persistent surfaces.
4. Keep the accepted 0.67 TSR primary fraction unless profiling proves another policy is required.
5. Re-run dual-visible correctness after each memory optimization.
```

This is the previously deferred STEP 1B.13B class of work and is now justified by measured runtime memory pressure.

## Remaining distance

For the specific visible defect "two portals on screen but one becomes black":

```text
SOLVED / MV-B PASS
```

For a production-ready full-fidelity renderer:

```text
1 remaining integration sub-step:
    MV-C-B system lifecycle integration + focused correctness regression

1 required optimization track:
    MV-D VRAM / bounded secondary rendering
```

Portal-on-portal recursion >= 2 remains a later feature gate and is not required to close the current top-level display inconsistency.
