# Interior Portal — STEP 1B.5 Validation Handoff

Date: **2026-09-15**

This file is intentionally small and current. It exists so the full-view-family
spike can be resumed without rewriting the historical global checkpoint.

Current branch work:

```text
a2204a3e307b5bef627289ffccbb094ef9c744ed
portal: add full view-family lit renderer spike

aab0927cda0aa4ec250d32991e4afe9d6e903d90
docs(portal): document full view-family lit spike

d3eb17c0c61522868634084d55d34fbff18c4dd1
portal: harden full view-family spike lifetime

fd5443d6e8bc0143e19c9f8fac9fba6bcb73a09d
docs(portal): update full view-family spike audit

9eda3aa84a487b3037aa74aaf2d156bfaede50db
portal: fix primitive id access in full view spike

7f08c23429dba76158bc3749e349010095ffdd18
portal: load renderer module explicitly for full view spike
```

Validation state:

```text
UE 5.8 PUBLIC API STATIC AUDIT PASS
REAL BUILD ATTEMPT 1: FAILED AT HiddenPrimitives COMPONENT ID ACCESS
FIXED IN 9eda3aa84a487b3037aa74aaf2d156bfaede50db
REAL BUILD ATTEMPT 2: FAILED BECAUSE GetRendererModule() IS NOT A DECLARED SYMBOL
FIXED IN 7f08c23429dba76158bc3749e349010095ffdd18
BUILD RERUN REQUIRED
SPIKE NOT YET RUN
NO VISUAL PASS CLAIMED
```

## Compile correction 1 — primitive renderer identity

The first real UE 5.8 compile found:

```text
InteriorPortalFullViewFamilySpike.cpp(82,53):
error C2039: 'ComponentId' is not a member of 'UPrimitiveComponent'
```

The spike had attempted:

```cpp
ViewInitOptions.HiddenPrimitives.Add(Primitive->ComponentId);
```

UE 5.8 exposes the renderer identity through the public
`UPrimitiveComponent::GetPrimitiveSceneId()` accessor. The code now uses:

```cpp
ViewInitOptions.HiddenPrimitives.Add(Primitive->GetPrimitiveSceneId());
```

## Compile correction 2 — renderer module access

The next real UE 5.8 compile reached the renderer submission point and found:

```text
InteriorPortalFullViewFamilySpike.cpp(320,3):
error C3861: 'GetRendererModule': identifier not found
```

`RendererInterface.h` exposes the `IRendererModule` interface, but this project
cannot rely on a global `GetRendererModule()` helper. The Renderer module is
already a private dependency of `SlayTheSpireDemo`, so the spike now acquires the
module explicitly through Unreal's normal module manager path:

```cpp
IRendererModule& RendererModule =
    FModuleManager::LoadModuleChecked<IRendererModule>(TEXT("Renderer"));
RendererModule.BeginRenderingViewFamily(&Canvas, &ViewFamily);
```

`Modules/ModuleManager.h` was added accordingly. This correction changes only
module lookup; it does not change the transformed view, clipping plane,
`FSceneViewFamily`, render target, readback or renderer-feasibility claim.

The earlier static audit confirmed that the UE 5.8 public API documents the
principal interfaces used by the spike. The installed UE 5.8 build remains the
authority for exact symbols and signatures, so compilation is being advanced
one real error at a time rather than changing renderer architecture speculatively.

Required next action:

1. Pull commit `7f08c23429dba76158bc3749e349010095ffdd18` or later.
2. Compile `SlayTheSpireDemoEditor Win64 Development` again.
3. If another compiler error appears, capture the first relevant error block.
4. If the build passes, run `/Game/House/L_Interior_LivingKitchen` in PIE/Game.
5. Place/link both portals and face one visible aperture.
6. Execute `portal.RunFullViewFamilySpike`.
7. Inspect:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

A visibly lit transformed image advances the work to integration with the
existing pre-tonemap aperture composition boundary. Compile/API failure or a
black output must be recorded before escalating to the renderer-private hook.

See `docs/InteriorPortalFullViewFamilySpike.md` for the complete claim boundary.
