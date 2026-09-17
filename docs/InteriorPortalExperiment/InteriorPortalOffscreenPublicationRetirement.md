# Interior Portal Offscreen Publication Retirement

Status: **PASS / USER PIE ACCEPTED (2026-09-16)**

## Symptom

With the FullFidelity renderer active, a portal could render correctly while visible and recursive rendering could also be correct, yet a fast camera turn that moved the portal out of the viewport could expose the portal's default spiral material for a frame.

The important reproduction detail was speed: slow camera motion might not reproduce it, while a rapid visible -> offscreen transition did.

## Root cause

This was not a projective-aperture failure and not a recursion failure.

`FMultiVisibleProducer::SubmitVisibleEndpoints()` evaluates endpoint visibility on the game thread from the current player camera. Before this fix, `HideLayer()` synchronously called `ClearRequest()` when an endpoint left the visible recursion set.

The render thread could still be processing a previously queued main/parent view family in which that portal was visible. Synchronous game-thread clearing therefore created an ownership gap:

1. previous player view was already queued and still contained the portal surface,
2. current game-thread visibility became false,
3. `ClearRequest()` immediately removed the completed FullFidelity publication,
4. the older queued view reached BeforeDOF and found no portal request,
5. the physical portal surface remained rasterized,
6. the default spiral fallback became visible for that frame.

This explains why the regression was strongly correlated with fast camera motion rather than with a particular grazing angle.

## Root fix

Publication retirement is ordered on the render queue rather than performed synchronously from visibility code.

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

## Runtime acceptance

User PIE validation after commit `b943ecc52a6f49cc7ddc5faf6494007a5096df92` reported the previously reproducible fast-camera regression as resolved.

Accepted behavior:

```text
visible portal
 -> rapid camera turn
 -> portal fully leaves viewport
 -> rapid return
```

No longer exposes the default spiral fallback during the transition.

Recursive FullFidelity rendering remained functional; the user had already confirmed that `RecursionDepth=2` can show the nested portal before physically crossing.

## Accepted invariants

During a visible -> offscreen transition:

- an already queued visible view retains its matching completed publication,
- an offscreen future view does not receive a stale publication,
- an old extraction callback cannot republish after retirement generation advances,
- a newly visible generation survives an older queued retirement command,
- no default spiral frame appears merely because the game-thread visibility result changed before the render thread consumed the prior view.

## Gate result

```text
FAST VISIBLE -> OFFSCREEN SPIRAL FLASH = PASS
FAST OFFSCREEN -> VISIBLE RETURN = PASS
RECURSIVE PUBLICATION RETIREMENT REGRESSION = NOT OBSERVED
PUBLICATION RETIREMENT MODEL = ACCEPTED
```

Keep this transition in focused visual regression coverage whenever request ownership, visibility culling, recursion publication, or render-thread scheduling changes.
