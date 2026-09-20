# P1A-3/P1A-4 runtime capacity retirement

Date: 2026-09-21. Current evidence HEAD: `5b6ad649d3e3d8977a79596132168d5067bdd0b2`.
P1A-4 implementation commit: `787e507425131f03114794ea1ab3d91b5fdb1cc1`.
Focused trace-instrumentation commit: `5b6ad649d3e3d8977a79596132168d5067bdd0b2`.
Scope: ViewState capacity retirement and its dependent publishers, not the full
P1 performance seal. The preceding crop visual gate was confirmed by the user.

## Implemented contract

Capacity processing runs before linked-pair, player, viewport and visibility
early returns. Levels at or above RequestedDepth immediately stop accepting
submissions/publications and move into endpoint-owned retirement records.
The active slot becomes a fresh unallocated object; queued extraction callbacks
retain the old object's address until its retirement fence completes.

Retirement detaches both the level publisher and the child composition extension
that retains its ViewState pointer. Publication clears target those detached
objects on the render queue, never their replacements. An RHI-thread-depth
FRenderCommandFence follows the clears. Polling completes retirement without
Wait or FlushRenderingCommands in the capacity-change path. Only then are the
ViewState and owned legacy depth target released through UE's normal resource
lifetime mechanism. The fence is not a measurement of GPU-resident memory release.

An immediate expansion creates a new lifetime and fresh dependent extensions;
retained L0 is not rebuilt. At most two allocated lifetimes per endpoint/level
are permitted: one retiring plus one reusable, or two retiring. When both are
pending, allocation is deferred without blocking the game thread; submission
uses the ready consecutive prefix. Visibility alone does not retire ownership.
Stop still uses the existing synchronous teardown boundary and drains retirees.

The v3 diagnostic report counts retired ViewStates in total ownership and exposes
`retiringLifetimes` with level, identity and fence-completion observation. Layer
masks/counts describe distinct levels; the retirement array describes individual
old generations and may overlap active levels during expansion. RECLAIMABLE is
a transient game-thread state consumed during polling, not a permanent cache.

Shared Ping-Pong targets and scratch are retained. Legacy color-array shrinking,
shared slot shrinking and their complete capacity gates remain P1A-5/P1A-6/P1A-7.
This change does not claim that all resources fit the new depth budget yet.
TSR/Lumen quality, crop geometry, gameplay, map assets and the default switch
value are unchanged.

## Validation evidence

- UE 5.8 bundled .NET project generation passed:
  `Saved/Logs/PortalP1A4ProjectFiles.log`.
- Editor build passed: `Saved/Logs/PortalP1A4FinalBuild.log`. The first build
  found a const mismatch against UE's non-const ViewState getter; corrected
  before the successful build. The final adjustment clears detached publishers
  under their mutex without changing their game-thread enabled flag.
- Focused NullRHI Automation: **10 passed, 0 failed, 0 not run**.
  `Saved/AutomationReports/PortalP1A4/index.json`,
  `Saved/Logs/PortalP1A4Automation.log`. Includes the new capacity reversal
  policy test and existing lifetime, crop, exposure and ownership coverage.
These test CPU contracts, not GPU resource execution. The final publisher-clear
  adjustment is validated by the subsequent actual D3D12 run.
- Initial D3D12 matrix: `Saved/PortalP1A4Runtime.json`; Stop ownership report:
  `Saved/PortalP1A4Stopped.json`. All eight warmed endpoint ViewStates shrink
  to two; rapid reversal observes three retirees alongside replacement layers;
  Stop reports zero ViewStates and zero explicit targets. Its timings are
  excluded because editor background throttling affected the run.
- Final-build D3D12 matrix and RHI snapshots:
  `Saved/PortalP1A4FinalRuntime.json`, `Saved/Logs/PortalP1A4FinalRuntime.log`.
  Background throttling is disabled transiently for this run. Initial Python
  setup attempts failed on reflected setting names before PIE; the corrected
  script uses the native property name. No failed attempt is passing evidence.

The fixture moves only PIE instances into a facing pair, exercises depth changes
and alternating endpoint visibility, and ends PIE without saving the map. A
requested depth of four is counted only when submitted/published masks show
all four actual layers. Dump and Python sampling add overhead; frame deltas
are editor/world frame observations, not GPU timestamp timings or a controlled
before/after speedup measurement.

## Observed ownership and transition cost

Final-build matrix checks passed for stable L0 identity, distinct rebuilt L1-L3
identities, no more than two owned generations per endpoint/level, offscreen
capacity retirement, and a new four-layer publication while old layers retire.
Both endpoints were warmed by alternating the viewing direction; they were not
simultaneously visible at four layers. Targets were 1920 x 731, Ping-Pong on,
D3D12 on the RTX 4060 Laptop GPU. The facing-pair fixture was PIE-only.

| Observation | Owned ViewStates | Retiring lifetimes | Submitted layer masks (blue, orange) |
|---|---:|---:|---|
| Both endpoints warmed by alternating views | 8 | 0 | 0, 15 |
| Lower depth while blue is offscreen, early | 8 | 6 | 0, 1 |
| Same shrink, settled | 2 | 0 | 0, 1 |
| Rapid expansion before previous retirement completes | 8 | 3 | 15, 0 |
| Rapid expansion, settled | 5 | 0 | 15, 0 |
| Stop | 0 | 0 | 0, 0 |

Separate RHI category snapshots in the final-build log:

| RHI category | Warm four-layer view | Settled first shrink to one |
|---|---:|---:|
| UAV Texture Memory | 3114.211 MB | 2174.164 MB |
| Render Target 2D Memory | 985.062 MB | 907.625 MB |

These are process-wide category observations, not portal-exclusive memory,
physical GPU residency, peak budget measurements or a percentage improvement.
Do not sum overlapping RHI categories into an invented total.

The first final-build frame samples revealed engine/Slate delta clamping. A
focused Stop -> Restart replay therefore records `time.perf_counter()` intervals
in `Saved/PortalP1A4TimingRuntime.json`; its zero-ownership teardown is in
`Saved/PortalP1A4TimingStopped.json`. Same map/fixture and target dimensions:

| Transition interval | Samples | Median wall interval | Maximum wall interval |
|---|---:|---:|---:|
| 4 -> 1 | 65 | 21.24 ms | 61.48 ms |
| Stable 1 -> 4 | 65 | 46.31 ms | 73.31 ms |
| Rapid 4 -> 1 -> 4 re-expansion | 65 | 56.91 ms | 1035.51 ms |
| Final 4 -> 1 | 65 | 23.01 ms | 65.21 ms |

These callback intervals include editor, diagnostics and rendering work; they
are not GPU timings or isolated retirement CPU cost. Startup warmup had a
1573.43 ms maximum and is not steady-state evidence. The approximately 1 s wall
interval therefore remained an open attribution question until the focused
Unreal Insights pass below. It must not be interpreted as one second spent in
P1A-4 lifetime-management code.

### Focused Unreal Insights transition evidence — 2026-09-21

A follow-up manual PIE capture on the current `portal/full-fidelity-p1` branch
used `portal.FullFidelityPingPong 1` and temporary CPU trace scopes added by
`5b6ad649` around the P1A-4 capacity/lifetime path. The user reproduced
`Depth 1 -> 4` and captured the actual expansion frame where both endpoint
L1/L2/L3 ViewStates were recreated.

Focused expansion-frame CPU observations:

| Trace scope | Count | Inclusive time |
|---|---:|---:|
| `Portal_ViewStateAllocate` | 6 | 21 us |
| `Portal_CreateRecursiveExtension` | 6 | 25.9 us |
| `Portal_EnsureLayerViewState` | 6 | 48.3 us |
| `Portal_SubmitVisibleEndpoints` | 1 | 591.4 us |
| `Portal_SubmitLayer` | 8 | 471.9 us |
| `Portal_UpdateCapacity` | 1 | 800 ns |
| `Portal_PollRetirements` | 2 | 100 ns |

The six ViewState/extension creations match two endpoints times rebuilt
L1/L2/L3. This evidence rules out `ViewState.Allocate`, recursive extension
creation, capacity update and retirement polling as the source of the previously
observed approximately one-second wall interval. The complete Portal CPU submit
path on the captured expansion frame remained sub-millisecond.

The same manual Insights session also compared stable rendering after capacity
settled:

| Stable sample | Rendering frame time | Approximate frame rate |
|---|---:|---:|
| RequestedDepth = 1 | 31.34 ms | 31.9 fps |
| RequestedDepth = 4 | 145.78 ms | 6.9 fps |

The depth-four sample is about 4.65x the depth-one frame time. Additional
sustained depth-four captures in the same session showed roughly 215-262 ms
rendering/GPU frames. Those broader samples are qualitative evidence of sustained
high recursion rendering cost, not a controlled benchmark and not a P1A-4
retirement cost measurement.

The depth-four traces are dominated by repeated full scene rendering work
(`SceneRenderer` / `RenderGraphExecute` / `Scene`, including Lumen, lighting,
VSM/Nanite/BasePass-related work), while Portal lifetime-management scopes remain
microsecond to sub-millisecond scale. Therefore the sustained depth-four frame
cost belongs to recursion rendering policy/performance work, primarily the later
P1B screen-coverage/effective-depth gate, rather than to P1A-4 queue-safe
retirement.

**P1A-4 transition-performance attribution is therefore CLOSED for the CPU
lifetime-management path.** This does not claim that RequestedDepth=4 has
acceptable steady-state performance, nor does it seal the full P1A/P1B/P1C
program. The separate lifecycle-transition visual acceptance below remains a
manual gate until explicitly confirmed by the user.

Fallback-mode check (`portal.FullFidelityPingPong 0`), limited to depth two:
`Saved/PortalP1A4FallbackFixedRuntime.json` confirms `2 -> 1 -> 2` and rapid
reversal. ViewStates and legacy depth targets decrease from four to two;
re-expanded blue layers submit/publish mask 3. Legacy color targets remain four,
as expected before P1A-5. `Saved/PortalP1A4FallbackFixedStopped.json` records
zero ViewStates/targets after Stop. The preceding fallback probe lost camera
visibility during initial warmup and failed its visibility assertion; it is
not a passing fixed-camera baseline. The corrected fixture pins and records
camera pose. Neither run attempts the previously failing full-view depth four.

The observed rapid-reversal snapshots prove old/new coexistence, not saturation
of every queue slot. The two-retiree allocation-backpressure bound is covered
by the deterministic policy test; no artificial render-thread delay was added
to production solely to force that branch in PIE.

## Remaining gate boundaries

**USER ACTION REQUIRED — visual transition acceptance:** in
`/Game/House/L_Interior_LivingKitchen`, enable Ping-Pong before a new PIE, warm
actual recursive views, change depth `4 -> 1 -> 4`, including quick reversal and
turning an endpoint offscreen. Expect stable retained L0, no stale scene flash,
no prolonged blank aperture and correct gun occlusion. Record the result or a
clip of any failure. The prior crop acceptance does not automatically accept
these new lifetime transitions.

Full-view depth-4 OOM remains an earlier open limitation. Focused Insights now
shows that the previously suspected transition hitch is not caused by the P1A-4
CPU lifetime path, while stable depth-four recursion remains very expensive and
is carried forward as a P1B performance baseline. This step does not prove the
complete P1A/P1B/P1C matrix, packaged smoke or all target shrinking. No full P1
seal is claimed.
