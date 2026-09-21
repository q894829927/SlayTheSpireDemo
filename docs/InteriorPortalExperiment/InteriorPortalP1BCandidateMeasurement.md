# P1B Candidate Threshold Measurement

Date: 2026-09-22  
Branch: `portal/full-fidelity-p1`  
Authority: `docs/PortalPerformanceVRAMP1Plan.md §8, §10`  
Status: **M1-M5 PASS / PRODUCTION DEFAULT 0.0025 IMPLEMENTED / FINAL REGRESSION PENDING**

## Purpose

Do not begin with GPU profiling for all candidate thresholds.

First identify fixed camera positions where the candidate thresholds actually
change recursive submission count. Then run GPU A/B only on meaningful pairs.

Candidate thresholds:

~~~text
0
0.001
0.0025
0.005
~~~

Current accepted functional evidence already proves threshold=0 parity, forced
cutoff, L0 survival, stale-child clearing, persistent-capacity retention,
same-lifetime recovery, build PASS, and FullFidelity Automation 11/11 PASS.

## Fixed conditions

Keep constant during one comparison set:

~~~text
same PIE session when practical
same map
same viewport/window size
same graphics settings
same portal transforms
same camera transform
same RequestedDepth
same portal.FullFidelityPingPong value
same PrimaryResolutionFraction
same warm-up rule
~~~

Do not resize the window between threshold samples.

## Phase M1 — find a discriminating small-portal camera

The previously accepted Depth=2 camera is not useful for candidate discrimination:

~~~text
Endpoint 0 L1 ParentCoverage ~= 0.0076
Endpoint 1 L1 ParentCoverage ~= 0.0088
~~~

All production candidates are <= 0.005, so that camera should retain L1.

Use the current safe fallback configuration first:

~~~text
RequestedDepth = 2
portal.FullFidelityPingPong 0
portal.MinRecursionScreenCoverage 0
~~~

Move/rotate until at least one visible endpoint's L1 ParentCoverage is in:

~~~text
0.001 .. 0.005
~~~

Preferred first target is 0.002 .. 0.004 because one fixed camera can then
distinguish more than one candidate.

Do not change the camera after selecting the measurement position. Record the
threshold-zero dump.

## Phase M2 — submission screening at the exact same camera

For each candidate, normalize hysteresis state before measuring:

~~~text
portal.MinRecursionScreenCoverage 0
wait until L1 is included / stable

portal.MinRecursionScreenCoverage <candidate>
wait until stable
portal.DumpMultiVisibleTSRSpike
~~~

Run this for 0.001, 0.0025 and 0.005. The reset to zero makes each nonzero
candidate start from the same included state instead of inheriting threshold
history from the previous candidate.

| Threshold | E0 L1 coverage | E0 EffectiveDepth | E0 Submissions | E0 cutoff | E1 L1 coverage | E1 EffectiveDepth | E1 Submissions | E1 cutoff | Visual |
|---:|---:|---:|---:|---|---:|---:|---:|---|---|
| 0 | 0.014953 | 2 | 2 | NONE | 0.001617 | 2 | 2 | NONE | baseline |
| 0.001 | 0.014953 | 2 | 2 | NONE | 0.001617 | 2 | 2 | NONE | unchanged |
| 0.0025 | 0.014953 | 2 | 2 | NONE | 0.001617 | 1 | 1 | SCREEN_COVERAGE L1 | cutoff |
| 0.005 | 0.014953 | 2 | 2 | NONE | 0.001617 | 1 | 1 | SCREEN_COVERAGE L1 | same submission result as 0.0025 |

Expected exit boundary when starting from included state:

~~~text
0.001  -> 0.0009
0.0025 -> 0.00225
0.005  -> 0.0045
~~~

After exclusion settles, the displayed re-entry boundaries are 0.0011,
0.00275 and 0.0055 respectively.

## Phase M3 — choose meaningful GPU A/B pair

M2 user evidence selects the pair for this fixed camera:

```text
baseline = 0
candidate = 0.0025
```

Reason:

```text
0.001  -> no SubmissionCount reduction
0.0025 -> Endpoint 1 falls from 2 submissions to 1
0.005  -> same Endpoint 1 submission result as 0.0025
```

Endpoint 0 remains at EffectiveDepth=2 / SubmissionCount=2 across all four
thresholds, so the policy is selectively cutting only the small recursive child
at Endpoint 1 in this camera.

Do not profile a threshold that leaves SubmissionCount identical to zero.

Choose baseline=0 and the lowest nonzero threshold that materially reduces
submissions in the selected small/distant camera.

If none of 0.001 / 0.0025 / 0.005 reduces submissions, move farther or make the
nested portal smaller and repeat M1/M2. Do not expand the candidate set just to
force a result.

## Phase M4 — warm GPU A/B — USER CAPTURED PASS

Two 300-frame CSV captures were analyzed in the user-provided capture order:

```text
Profile(20260922_000003).csv -> threshold 0 baseline
Profile(20260922_000157).csv -> threshold 0.0025 candidate
```

Both files contain exactly 300 valid `GPUTime` samples.

Measured GPUTime:

| Metric | threshold 0 | threshold 0.0025 | Delta | Change |
|---|---:|---:|---:|---:|
| Average | 50.270 ms | 36.745 ms | -13.525 ms | -26.90% |
| Median | 50.222 ms | 36.650 ms | -13.573 ms | -27.03% |
| P95 | 51.148 ms | 37.720 ms | -13.428 ms | -26.25% |
| P99 | 52.288 ms | 38.741 ms | -13.547 ms | -25.91% |

The full frame-time CSV statistic also drops from 54.449 ms average to
40.111 ms average (-14.338 ms / -26.33%). GameThreadTime is nearly unchanged
(7.527 -> 7.317 ms average), while RenderThreadTime drops from 54.425 to
40.101 ms average. This is consistent with the measurement being dominated by
rendering workload rather than gameplay-thread work.

The GPUTime distributions are well separated in these fixed 300-frame windows:

```text
baseline min/max  = 48.893 / 54.422 ms
candidate min/max = 35.623 / 40.876 ms
```

Associated shared GPU CSV categories with the largest average decreases include
ShadowDepths, LumenSceneUpdate, TemporalSuperResolution and RayTracingScene.
These category deltas are supporting attribution only; they are not summed
because GPU stat scopes may overlap.

This A/B corresponds to the already-recorded workload change at the same
camera:

```text
Endpoint 1:
threshold 0      -> EffectiveDepth=2 / SubmissionCount=2
threshold 0.0025 -> EffectiveDepth=1 / SubmissionCount=1

Endpoint 0:
unchanged at EffectiveDepth=2 / SubmissionCount=2
```

For the selected fixed camera and threshold pair:

~~~text
warm-up >= 300 frames
sample >= 300 frames
~~~

Record where tooling permits: GPU average, median, P95, P99, SubmissionCount
per endpoint, EffectiveDepth per endpoint, cutoff-level ParentCoverage, and
visual observation. If a statistic is unavailable, mark it unavailable.

## Phase M5 — visual stability matrix — USER CONFIRMED PASS

The user completed the manual candidate-threshold visual checks with
`portal.MinRecursionScreenCoverage=0.0025` and reported no P1B visual problems
for the tested fixed-portal cases:

~~~text
large nested portal
tiny/distant nested portal
bright/high-contrast tiny nested portal
oblique portal
slow 1 -> 2 threshold recovery
slow 2 -> 1 threshold cutoff
look-away / look-back
~~~

Observed acceptance:

~~~text
no rapid cutoff flicker
no stale nested child
no persistent black aperture
no unexpected spiral flash
no repeated EffectiveDepth oscillation
L0 remains usable
~~~

A separate observation exists when physically re-placing an endpoint: the old
portal location can appear white and fade away. Current placement code moves the
same endpoint actor immediately and invalidates portal renderer histories, so
that replacement-only temporal residue is tracked separately from P1B. It was
not reproduced by fixed-portal coverage threshold crossings and does not block
this M5 gate.

M1-M5 evidence is complete. The user accepted `0.0025` as the production
default, and the runtime CVar default was changed from `0` to `0.0025` in
commit `7bee7cd79426`. The cutoff algorithm and 10% hysteresis are unchanged.

Only a final minimal build + focused FullFidelity Automation regression remains
before P1B can be sealed.
