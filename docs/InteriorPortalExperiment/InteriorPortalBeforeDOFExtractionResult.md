# Interior Portal — STEP 1B.6 BeforeDOF Extraction Result

Date: **2026-09-15**

Status:

```text
STEP 1B.6 BEFORE-DOF EXTRACTION = PASS
FULL TRANSFORMED SECONDARY VIEW = PASS
BEFOREDOF CALLBACK EXECUTION = PASS
LINEAR HDR SCENECOLOR EXTRACTION = PASS
MAIN-VIEW COMPOSITION = NOT YET PROVEN
```

## Runtime evidence

The successful run reported:

```text
status = BEFOREDOF_OUTPUT_WRITTEN
sceneViewIsSceneCapture = false
targetFormat = PF_FloatRGBA / RTF_RGBA16f
targetSize = 1745 x 876
beforeDOFCallbackExecuted = true
```

Both the final full-view-family output and the secondary BeforeDOF SceneColor
were written as PNG + EXR.

The two images contain the same transformed target-space scene content. The raw
BeforeDOF PNG appears even more clipped/white than the final PNG. That is expected
for this diagnostic and must not be treated as an extraction failure: the
BeforeDOF texture is linear HDR SceneColor and is being written directly to an
LDR PNG without the player's final tone mapping.

Direct inspection of the uploaded float EXR files showed approximately:

```text
Final RGB:
  max      ~44.84
  median   ~4.68

BeforeDOF SceneColor RGB:
  max      ~3858
  median   ~18.56
  90th pct ~773.5
```

The large >1 range is the important evidence. STEP 1B.6 has successfully crossed
from a final/display-like output into a pre-tonemap HDR scene-color domain.

The BeforeDOF alpha channel is not suitable as a portal opacity contract (large
areas are zero). Future composition must therefore use the analytic portal
aperture mask for ownership and preserve the main SceneColor alpha rather than
lerping the extracted alpha into the main frame.

## Consequence

Do not add brightness, gamma or exposure multipliers to make the standalone
BeforeDOF PNG look correct. The intended architecture remains:

```text
full transformed secondary renderer
    -> BeforeDOF linear HDR SceneColor
    -> portal HDR render target
    -> main player view BeforeDOF analytic aperture composition
    -> one player-view exposure / tone-map authority
```

The immediate next gate is STEP 1B.7: reuse the validated extraction target as
the input to the already validated main-view BeforeDOF aperture compositor. The
first 1B.7 proof should be one-shot/static so renderer-domain correctness is
separated from per-frame lifetime, temporal history and performance work.
