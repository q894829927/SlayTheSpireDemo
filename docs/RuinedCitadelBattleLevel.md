# Ruined Citadel Battle Level

Implemented through the connected Unreal Editor MCP on **2026-09-10**.

## Result

`/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel` is a new copy of the
production Native `L_BattleTest` assembly. Both `GameDefaultMap` and
`EditorStartupMap` in `Config/DefaultEngine.ini` now point to this new map.
Existing battle maps and the shared Native HUD assets were not saved or changed.

The supplied image is displayed behind the battle HUD during play. The background
is a viewport widget, so the editor's ordinary 3D level viewport still shows the
copied test scene when PIE is stopped. Press Play to see the battle composition.

## Assets and behavior

- Source: `Content/SlayTheSpireDemo/UI/images/backgrounds/Battle_RuinedCitadel.png`.
  This is an unchanged copy of the user's PNG, **1672 x 941** pixels, SHA-256
  `07E98E42A10A80556E9338D9F35C65095F5ED9BD45AE45FA00C8ED8353E5EBE1`.
- Texture: `/Game/SlayTheSpireDemo/UI/Textures/Backgrounds/T_Battle_RuinedCitadel`.
  UI texture group, sRGB, UI RGBA compression, no mipmaps, never streamed.
- Widget: `/Game/SlayTheSpireDemo/UI/Widgets/Backgrounds/WBP_BattleBackground_RuinedCitadel`.
  A centered Image inside a ScaleBox uses `ScaleToFill`, both scaling directions,
  viewport clipping and `HitTestInvisible`. The original aspect ratio is preserved;
  different screen proportions crop the excess around the center rather than stretch.
- Actor: `/Game/SlayTheSpireDemo/Blueprints/Presentation/BP_BattleBackground_RuinedCitadel`.
  BeginPlay validates the local PlayerController, creates and stores the background
  widget, then adds it at viewport Z order **-100**, below the existing HUD at **0**.
  EndPlay removes the widget after an IsValid check.

The new level retains its own BattleManager, player, enemy, level initialization
and Native Presenter references. Gameplay, C++, shared HUD and Legacy assets are unchanged.

## Level cleanup

The copied template-only scene content was removed from this map through Unreal MCP:
`PlayerStart_0`, `DirectionalLight_0`, `SkyAtmosphere_0`, `SkyLight_0`,
`ExponentialHeightFog_0`, `VolumetricCloud_0`, `StaticMeshActor_0`, `Floor_0`,
`Brush_1` and `BuoyancyManager_0`. The Native HUD's gameplay/UI input setup was
kept because card selection, End Turn and right-click cancellation still require it;
the shared `DefaultInput.ini` was not changed. Engine-generated helpers
(`DefaultPhysicsVolume`, debugger/Chaos helpers and `AbstractNavData`) remain owned
by the engine rather than the battle level.

The map `WorldSettings` now has `bEnableNavigationSystem=false`,
`bEnableAISystem=false` and `bForceNoPrecomputedLighting=true`, so the removed
navigation, AI and baked-lighting template systems are not requested by this level.

## Validation

- MCP texture import, widget compile, actor graph compile and explicit saves: **PASS**.
- Saved map loads and retains the new background actor: **PASS**.
- Runtime Presenter uses `WBP_BattleHUD_Native_C`, with a valid WidgetInstance,
  ViewModel, PresentationController and BattleManager in the new PIE world: **PASS**.
- Focused floating-window PIE visual check: **PASS**. The image fills the game area,
  with characters, vitals, relics, cards and controls rendered above it.
- End Turn was activated through Slate input. Logs show the player turn ending,
  enemy turn, and next player turn at revision 9; the final screenshot shows HP
  **74/80**, energy **5/5** and the next hand over the background: **PASS**.
- Runtime/cleanup log query returned no `Blueprint Runtime Error`, `Accessed None`
  or `ResolutionFault` entries. PIE was stopped after the check.
- After the template cleanup, the MCP actor query returned no `Light` or `PlayerStart`
  actors and the saved map retained only the battle/background assembly plus engine
  helpers. A post-cleanup PIE log reached `GameModeBase`, Enhanced Input subsystem
  initialization, deck/battle initialization and a request-eligible player turn.
  The expected no-`PlayerStart` engine message was logged; the MCP transport then
  disconnected before a second visual capture, so the earlier visual evidence is
  retained as the UI/background check.
- MCP settings readback and the INI diff confirm both default map settings.
  Direct saved asset dependencies establish Map -> background Actor -> Widget ->
  Texture, with no added Legacy asset reference.

Local evidence: `Saved/Validation/RuinedCitadel/BattleBackground.png`,
`Saved/Validation/RuinedCitadel/EditorSession.log` and the post-cleanup
`Saved/Validation/RuinedCitadel/CleanupPIE.log`. The initial in-viewport attempt
was obscured by an automatic source-import notification; the successful visual
check used a floating PIE window. No C++ build, Automation suite or packaged-game
validation was required or performed for this asset/configuration change.

This check does not seal the separate pending character-animation or hand-layout
acceptance gates. To return to the earlier startup selection, set the two map
settings back to `/Game/SlayTheSpireDemo/Maps/L_BattleTest.L_BattleTest`.
