# Interior Portal — STEP 1B.14D-C Production TSR Parity Validation

Date: **2026-09-16**

State:

```text
STEP 1B.14A VISUAL-PARITY ISOLATION = PASS / OUTCOME B
STEP 1B.14B MAIN-VS-SECONDARY DIAGNOSTICS = PASS / ROOT CAUSE CLASSIFIED
STEP 1B.14C SECONDARY EYE ADAPTATION A/B = PASS
STEP 1B.14D EXACT PER-SUBMISSION COLOR SAMPLE OWNERSHIP = IMPLEMENTED / PUBLICATION ORDER HARDENED
STEP 1B.14D-C ATTEMPT 1 = FAIL / PARITY OBSERVER FROZE PIE
STEP 1B.14D-C ATTEMPT 2A = PASS / FREEZE REGRESSION CLOSED
STEP 1B.14D-C ATTEMPT 2B = FAIL / IN-FLIGHT COLOR SAMPLE PUBLICATION
STEP 1B.14D-C ATTEMPT 3A = PASS / TELEMETRY + STATIC BLACK-APERTURE REGRESSION CLOSED
STEP 1B.14D-C ATTEMPT 3B = USER DYNAMIC VISUAL MATRIX REQUIRED
```

## Scope of this gate

This gate closes the production single-layer TSR exposure/composition path without
introducing a scene-specific brightness multiplier or a second final-color exposure
owner.

The accepted ownership is:

```text
player main FSceneView
    -> owns final player exposure / tone mapping

portal secondary FSceneViewFamily
    -> owns its persistent temporal ViewState
    -> runs TSR
    -> runs EyeAdaptation
    -> extracts post-TSR / pre-tonemap HDR at Tonemap
    -> records the exact secondary PreExposure in the same submission's FColorSample

main BeforeDOF composition
    -> consumes only a completed portal submission
    -> converts HDR domains with MainPreExposure / SecondaryPreExposure
```

The purpose is correctness of renderer ownership and visual parity. Constant portal
gain is not an acceptance mechanism.

## Attempt 1 — FAIL: secondary observer interference

The first production-parity probe installed another world-scoped
`FWorldSceneViewExtension` and subscribed to Tonemap for both the player main family
and the manually constructed portal additional family.

Runtime symptom:

```text
portal.StartProductionParityValidation
-> PIE remained permanently displayed on one frame
```

This was a rendered-frame freeze, not console/input focus.

Root classification:

```text
SECONDARY VIEW-FAMILY OBSERVER INTERFERENCE
```

Fix:

```text
InteriorPortalProductionParityValidation.cpp
-> main-view-only observer
-> explicitly ignores additional/non-main view families
-> secondary telemetry remains producer-owned
```

Result: subsequent PIE runs continued rendering for thousands of frames. The freeze
regression is closed.

## Attempt 2B — FAIL: in-flight FColorSample publication

After the observer fix, the full TSR producer itself was healthy but the physical
portal aperture could remain black.

Composition diagnostics showed the key failure pattern:

```text
PortalComposition Subscribe ... RequestValid=1 HasTarget=1 HasDepthTarget=1
PortalComposition Execute ...
<no ComposeReady>
```

The request and targets existed and the main BeforeDOF callback executed. Therefore
this was not an aperture-discovery, visibility or callback-subscription failure.

### Root cause

The old `FTSRPortalProducer::SubmitFrame()` ordering was:

```text
1. create FColorSample(PreExposure=0)
2. queue secondary FSceneViewFamily
3. immediately publish Request to the main compositor
4. later run secondary Tonemap and fill FColorSample::PreExposure
```

The main BeforeDOF callback could consume the request between steps 3 and 4.
`TryGetExposureScale()` intentionally rejects an unfinished sample, so composition
returned the original main SceneColor. Because the physical portal surface under the
composition is black, repeated rejection appeared as a black portal.

Occasional successful frames were also explained: when scheduling allowed secondary
Tonemap to finish before main composition, `ComposeReady` appeared with sane exposure
ratios.

## Attempt 3 fix — completed-request publication

Commit:

```text
63a7534fcf3d8067d2c2630f1364554f2a9db40a
portal: publish TSR request only after extraction
```

The accepted ordering is now:

```text
1. build request and attach color/depth target identities
2. allocate exact per-submission FColorSample
3. queue the secondary view family
4. secondary Tonemap measures its current PreExposure
5. queue matching color extraction and secondary-depth transport in the same RDG graph
6. write that exact FColorSample
7. publish the completed Request from the render thread
8. main BeforeDOF consumes only the latest completed request
```

The legacy one-frame CVar bridge is not restored. There is no fallback magic gain.

## Attempt 3A runtime evidence — PASS

The user rebuilt the corrected branch and ran the normal TSR producer plus the
main-only parity observer.

### Main-view telemetry

Representative startup observations:

```text
EyeAdaptation=1
AA=4 (AAM_TSR)
PreExposure finite and positive
CameraCut=0
```

Final parity dump:

```text
MainFrames=725
MainPre=0.00139136089
Eye=1
AA=4
InvalidMain=0
MainCuts=0
Classification=MAIN_TELEMETRY_READY_CHECK_TSR_REPORT_AND_MANUAL_VISUAL_GATE
SecondarySource=PortalFullViewFamilyTSRSpike
```

Result:

```text
MAIN VIEW OWNERSHIP = PASS
INVALID MAIN PRE-EXPOSURE = 0
```

### Secondary TSR producer telemetry

Final producer dump:

```text
Submitted=726
Skipped=0
LastExtractionFrame=1324
CameraCuts=2
ContinuousHistoryFrames=724
Input=1920x902
ObservedAA=4
Jitter=(-0.000388500397,-0.000550964265)
JitterObserved=1
DepthFrame=1324
DepthSource=1286x604
DepthTarget=1920x902
```

Result:

```text
SECONDARY PRODUCER CONTINUITY = PASS
TSR = PASS
TEMPORAL JITTER = PASS
COLOR EXTRACTION ADVANCING = PASS
DEPTH TRANSPORT ADVANCING = PASS
VISIBLE-RUN SKIPS = 0
CAMERA CUTS = sparse / explainable
```

### Composition publication-order proof

The supplied `portal.CompositionDiagnostics 1` window spans frames 1162..1324.
A complete scan of that window gives:

```text
PortalComposition Subscribe   = 158
PortalComposition Execute     = 158
PortalComposition ComposeReady= 158
PortalComposition DrawQueued  = 158
PortalComposition ComposeSkip = 0
Execute without ComposeReady  = 0
```

This is the decisive regression result. The previous long run of valid
`Subscribe + Execute` callbacks with unfinished exposure metadata is gone.

Representative first diagnostic frame:

```text
Frame=1162
MainPreExposure=0.00144088385
SecondaryPreExposure=0.00133863057
ExposureScale=1.07638645
Projective=1
ProjectiveValid=1
SecondaryDepthTextureValid=1
DrawQueued=YES
```

Subsequent frames remain in the same sane exposure domain and continue to reach
`ComposeReady` and `DrawQueued` every sampled visible frame.

### Static visual evidence

The supplied PIE screenshot visibly contains the remote destination scene inside the
elliptical portal aperture. The persistent black-aperture failure from Attempt 2B is
no longer present, and the remote scene is no longer catastrophically dark.

Classification:

```text
ATTEMPT 3A = PASS
COMPLETED-SAMPLE PUBLICATION ORDER = ACCEPTED
PERSISTENT BLACK APERTURE REGRESSION = CLOSED
STATIC SINGLE-LAYER VISUAL RESTORATION = ACCEPTED
```

This is intentionally **not** yet a claim that every dynamic visual condition has
passed.

## Attempt 3B — remaining manual dynamic visual matrix

The supplied Attempt 3 log does not contain evidence for:

```text
Crossing=1
NearClip=1
NO_VISIBLE_PORTAL / leave-visible-set interval
```

Therefore the following movement-dependent checks remain open:

```text
1. strong slant / grazing view
2. approach then retreat without crossing
3. portal completely leaves the viewport for about 2 seconds
4. return to the same portal and hold
5. cross once
6. stop after crossing and look back through the portal
```

Accept only if all of the following are visually true:

```text
no repeated black / near-black flash during ordinary visibility
no stale exposure-domain frame after leave-and-return
no obvious brightness discontinuity during approach / retreat
no crossing-specific large exposure pop
slant / grazing view preserves the accepted projective aperture and clipping behavior
remote scene remains materially consistent with direct destination viewing
```

During the leave-visible-set portion, `framesSkipped` may increase because
`NO_VISIBLE_PORTAL` is the intended producer behavior. That is not itself a failure.
The important behavior is deterministic history invalidation followed by a clean
camera-cut/reset when the portal becomes visible again.

## USER ACTION REQUIRED — dynamic visual gate only

No code rebuild is required merely for this remaining validation if the branch has not
changed since Attempt 3A.

Keep the accepted producer running, perform the movement matrix above, then dump:

```text
portal.DumpFullViewFamilyTSRSpike
portal.DumpProductionParityValidation
portal.StopProductionParityValidation
```

For this dynamic pass, report whether any of these were observed:

```text
black flash
stale frame on return
brightness pop on approach/retreat
brightness pop on crossing
aperture/clipping break at grazing angle
```

If none occur, STEP 1B.14D-C can be closed and STEP 1B.14E can start.

## Performance note observed during Attempt 3A

UE emitted repeated-console-lookup warnings for:

```text
r.EyeAdaptationQuality
r.EyeAdaptation.PreExposureOverride
```

The currently active production parity and TSR source paths do not directly look up
those two named CVars. This warning is therefore tracked as a separate profiling /
renderer-overhead item rather than a correctness blocker for this gate. Do not tune
exposure policy merely to silence this warning.

## Failure routing

```text
PIE freezes again
    -> audit world-scoped view-extension isolation before any exposure work

TSR extraction stops advancing
    -> secondary producer regression

visible frame has Execute but no ComposeReady
    -> completed-request/sample ownership regression

ComposeReady is continuous but portal becomes black
    -> inspect actual PortalTexture RGB / shader sampling

portal is lit but materially darker with sane exposure scale
    -> investigate additional-family Lumen/reflection history

leave-return causes stale/black frame
    -> audit target/sample lifetime and temporal-history reset

only aperture boundary fails
    -> return to aperture/depth/stencil integration, not exposure
```

## Gate after Attempt 3B PASS

Proceed to:

```text
STEP 1B.14E — production merge / cleanup + focused regression
```

1B.14E must retain:

```text
secondary EyeAdaptation ownership
persistent secondary temporal ViewState
post-TSR / pre-tonemap HDR extraction
exact per-submission FColorSample metadata
completed-request publication ordering
main-view-only parity instrumentation
additional-view-family isolation
```

Then:

```text
retire the late-latched CVar bridge from normal validation instructions
run the affected TSR / exposure / near-grazing / depth / stencil focused regressions once
record single-layer renderer parity as accepted
```

Only after this closure should STEP 1B.13B secondary transport/view bounding resume.
Recursion >= 2 and full physics remain later gates.
