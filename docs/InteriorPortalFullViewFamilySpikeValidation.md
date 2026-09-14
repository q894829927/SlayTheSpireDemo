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
```

Validation state:

```text
NOT YET BUILT
NOT YET RUN
NO VISUAL PASS CLAIMED
```

Required next action:

1. Regenerate UE 5.8 project files.
2. Compile `SlayTheSpireDemoEditor Win64 Development`.
3. Run `/Game/House/L_Interior_LivingKitchen` in PIE/Game.
4. Place/link both portals and face one visible aperture.
5. Execute `portal.RunFullViewFamilySpike`.
6. Inspect:

```text
Saved/AutomationReports/PortalFullViewFamilySpike.png
Saved/AutomationReports/PortalFullViewFamilySpike.exr
Saved/AutomationReports/PortalFullViewFamilySpike.json
```

A visibly lit transformed image advances the work to integration with the
existing pre-tonemap aperture composition boundary. Compile/API failure or a
black output must be recorded before escalating to the renderer-private hook.

See `docs/InteriorPortalFullViewFamilySpike.md` for the complete claim boundary.
