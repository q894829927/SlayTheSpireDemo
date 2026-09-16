# Interior Portal Offscreen Publication Retirement

Status: IMPLEMENTED / USER PIE VALIDATION REQUIRED

## Symptom

With the FullFidelity renderer active, a portal can render correctly while visible and recursive rendering can also be correct, yet a fast camera turn that moves the portal out of the viewport can expose the portal's default spiral material for a frame.

The important reproduction detail is speed: slow camera motion may not reproduce it, while a rapid visible -> offscreen transition does.

## Root cause

This was not a projective-aperture failure and not a recursion failure.

`FMultiVisibleProducer::SubmitVisibleEndpoints()` evaluates endpoint visibility on the game thread from the current player camera. Before this fix, `HideLayer()` synchronously called `ClearRequest()` when an endpoint left the visible recursion set.

The render thread may still be processing a previously queued main/parent view family in which that portal was visible. Synchronous game-thread clearing therefore created an ownership gap:

1. previous player view is already queued and still contains the portal surface,
2. current game-thread visibility becomes false,
3. `ClearRequest()` immediately removes the completed FullFidelity publication,
4. the older queued view reaches BeforeDOF and finds no portal request,
5. the physical portal surface remains rasterized,
6. the default spiral fallback becomes visible for that frame.

This explains why the regression is strongly correlated with fast camera motion rather than with a particular grazing angle.

## Root fix

Publication retirement is now ordered on the render queue rather than performed synchronously from visibility code.

When a visible layer becomes hidden:

1. its history/publication generation advances immediately,
2. old in-flight extraction callbacks are prevented from publishing because their generation no longer matches `ActivePublicationGeneration`,
3. a render command is enqueued to retire the already-published request,
4. render work that was queued before the retirement command can still consume the old request,
5. the retirement command clears the request only if its `RendererHistoryGeneration` is older than the active generation,
6. if the portal becomes visible again before retirement executes, the newly published generation is not removed by the stale retirement command.

This is queue ownership synchronization, not a frame-count grace period or visibility hysteresis.

The same rule is applied to:

- main-view level-0 publications,
- recursive parent/child publications.

## Analytic aperture preservation correction

During investigation, a separate implementation inconsistency was found in the recursion producer.

`FInteriorPortalRenderRequest::Build()` already creates the production analytic world-space portal-plane geometry in `ForegroundDepthReference`. The producer comment said it only updated the cosmetic surface bias, but the code rebuilt `ForegroundDepthReference` with `BuildScreenToPortalMapping()`, replacing the analytic representation with the legacy inverse homography.

That overwrite has been removed.

The producer now only writes:

```cpp
Request.ForegroundDepthReference.Row2.W = Entry->SurfaceVisualBias;
```

The logical portal center, normal, width axis, height axis and inverse half-extents remain the analytic data produced by `Build()`.

## Expected invariants

During a visible -> offscreen transition:

- an already queued visible view must retain its matching completed publication,
- an offscreen future view must not receive a stale publication,
- an old extraction callback must not republish after retirement generation advances,
- a newly visible generation must survive an older queued retirement command,
- no default spiral frame should appear merely because the game-thread visibility result changed before the render thread consumed the prior view.

## USER ACTION REQUIRED

Build the current `portal/full-fidelity-p1` branch, then run PIE with the normal automatic FullFidelity lifecycle.

Recommended setup:

```text
RecursionDepth = 2
portal.MultiVisibleDiagnostics 1
portal.CompositionDiagnostics 1
```

### Test A — fast visible -> offscreen

1. Start from a clearly visible portal with remote scene rendered correctly.
2. Rapidly rotate the camera until the portal completely leaves the viewport.
3. Repeat left/right at least 20 times.
4. Include several transitions starting near a grazing/slanted view.

PASS:

- no one-frame default spiral exposure,
- no black aperture flash,
- no stale portal image appearing elsewhere on screen.

### Test B — fast offscreen -> visible

1. Keep the portal just outside the viewport.
2. Snap the camera back so it becomes visible.
3. Repeat at least 20 times.

PASS:

- no default spiral warm-up frame before FullFidelity composition,
- no old exposure-domain frame,
- recursive image returns normally when `RecursionDepth >= 2`.

### Test C — recursion unchanged

With `RecursionDepth = 2`, verify a portal visible inside the level-0 remote scene remains visible after this lifetime change.

PASS:

- recursion remains visible without crossing first,
- `portal.DumpFullFidelityRenderer` reports level-1 submitted/completed work when the recursive portal is actually visible.

## Failure routing

If the default spiral still appears only during fast visible/offscreen transitions, capture `portal.CompositionDiagnostics 1` logs around the failing frames. The relevant distinction is now:

- `Subscribe RequestValid=0` while the portal surface is still visibly rasterized: publication retirement ordering is still wrong,
- `RequestValid=1` and `ComposeReady/DrawQueued` but spiral still wins: investigate surface/main-depth ordering rather than publication lifetime,
- recursive level disappears while level 0 remains correct: investigate recursive publication retirement separately.
