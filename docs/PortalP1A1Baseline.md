# Portal P1A-1 Resource Baseline

Date: **2026-09-17**

Branch:

```text
portal/full-fidelity-p1
```

Status:

```text
P1A-1 — COMPLETE / VALIDATED / SEALED
P1A-2 — ACTIVE
```

## Validation

User-confirmed local validation:

```text
SlayTheSpireDemoEditor Win64 Development = PASS
focused FullFidelity Automation = PASS
```

P1A-1 instrumentation implementation lineage:

```text
860a1eb6462d93793b9b00d08e3aa28a7476610f
portal: report full-fidelity recursion resource lifetimes

957dc95e8b9a8dd9c3435c2d35c7d5d1ccc46d9d
portal: harden P1A1 target format diagnostics

9776e7bb171021a5352a1247ae5bafb67e52dbc5
取消const
```

The last commit fixes UE5.8 const-correctness for `FSceneViewStateReference::GetReference()` in read-only diagnostics.

## Baseline captures

Both captures used:

```text
PrimaryResolutionFraction = 0.67
Portal target = 1920 x 815
shared scratch = one 1920 x 815 RGBA16f target
```

### RequestedDepth = 1

```text
viewStates = 8
colorTargets = 2
depthTargets = 2
explicitTargetEstimatedBytes = 50,073,600 bytes (~47.75 MiB)
```

Only one endpoint was visible/submitted at capture time, with:

```text
visibleDepth = 1
effectiveDepth = 1
attemptedLayerMask = 0b0001
submittedLayerMask = 0b0001
publishedLayerMask = 0b0001
```

Despite RequestedDepth=1, all eight startup ViewStates remained allocated.

### RequestedDepth = 4, fully warmed visible endpoint

```text
viewStates = 8
colorTargets = 8
depthTargets = 8
explicitTargetEstimatedBytes = 162,739,200 bytes (~155.20 MiB)
```

The visible endpoint reported:

```text
visibleDepth = 4
effectiveDepth = 4
attemptedLayerMask = 0b1111
submittedLayerMask = 0b1111
submissionCount = 4
publishedLayerMask = 0b1111
```

The offscreen endpoint still retained four color targets and four depth targets after previous use.

## Quantified explicit RT delta

Depth 1 -> Depth 4 adds six endpoint-level color/depth target pairs:

```text
RGBA16f color target = 12,518,400 bytes
R32f depth target    =  6,259,200 bytes
pair                 = 18,777,600 bytes (~17.91 MiB)

6 pairs              = 112,665,600 bytes (~107.45 MiB)
```

This is only the Portal-owned explicit RT estimate. It does not include TSR/Lumen/ViewState internal RHI history allocations and must not be presented as total GPU VRAM.

## P1A-1 conclusions

The baseline proves:

1. ViewState ownership is currently maximum-oriented at producer startup: 8/8 ViewStates exist even at RequestedDepth=1.
2. Color/depth targets grow as deeper recursion is exercised.
3. Previously allocated color/depth targets remain owned after a level or endpoint becomes inactive; visibility alone does not shrink ownership.
4. Structured diagnostics distinguish requested/visible/effective work, exact attempted/submitted/published masks, explicit target ownership and estimated target bytes.
5. Lifetime identity and retirement state are not yet authoritative runtime concepts; they are the responsibility of P1A-2.

## Next task

```text
P1A-2 — Explicit lifetime state and identity
```

P1A-2 first establishes a small deterministic lifetime model and Automation coverage before wiring it into the producer. Actual ViewState/RT reclaim remains deferred to P1A-3/P1A-4 and later P1A target-reclaim steps.
