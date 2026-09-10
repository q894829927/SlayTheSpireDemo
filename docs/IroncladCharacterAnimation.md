# Ironclad Character Animation

Last updated: **2026-09-10**

## Scope

The Native combatant presentation widget now plays the Ironclad character profile from
`Content/SlayTheSpireDemo/UI/images/characters/ironclad`. The implementation stays on the
Presentation side: Gameplay commits records, `UBattleHUDWidget` translates accepted records
into animation requests, and `UBattleHUDCombatantPresentationWidgetBase` owns only the local
texture-frame playback and render transform.

The existing `WBP_CombatantPresentation` contract is preserved. Its `Img_Character` image
remains the only character surface; the native code captures its authored brush, transform and
opacity as the fallback and never changes the Blueprint hierarchy.

## Source assets and bake

The supplied Spine data contains two authored animations:

| Source animation | Baked frames | Authored duration | Runtime use |
| --- | ---: | ---: | --- |
| `Idle` | 120 | 6.6666 s | looping idle |
| `Hit` | 8 | 0.3333 s | one-shot damage reaction |

The source does not contain a separate attack, victory or death animation. The source atlas was
baked offline into transparent PNG frames with the Steam game's Spine 3.4 runtime classes, then
imported as ordinary Unreal textures. The project has no Spine runtime dependency. Generated
source frames are retained under:

```text
Content/SlayTheSpireDemo/UI/images/characters/ironclad/generated/idle/
Content/SlayTheSpireDemo/UI/images/characters/ironclad/generated/hit/
```

Imported runtime textures are under:

```text
/Game/SlayTheSpireDemo/UI/Textures/Ironclad/Idle/
/Game/SlayTheSpireDemo/UI/Textures/Ironclad/Hit/
/Game/SlayTheSpireDemo/UI/Textures/Ironclad/corpse
```

`corpse.png` is a static 512x512 defeat pose. The 120 Idle frames and 8 Hit frames are 900x656
and keep the same transparent canvas, so swapping a frame does not create a vertical alignment
jump.

## Record-to-animation mapping

The HUD requests animation only after the corresponding Native Presentation record has passed
its payload and token validation and its finish timer has started:

| Record fact | Presentation id | Animation |
| --- | --- | --- |
| `CardPlayed` | `SourcePresentationId` | `Attack` |
| `Damage` | `TargetPresentationId` | `Hit` |
| `Victory` / `Defeat` | `WinnerPresentationId` / `DefeatedPresentationId` | `Victory` / `Defeat` |

`Attack` uses the authored Idle frames plus a reversible Blueprint-tunable lunge, and is requested
only for committed cards whose type is `Attack`. Skill, Power, Status and Curse cards do not move
the character. This makes the missing attack source explicit and gives attack card play a readable
cue without pretending that the `Hit` reaction is an attack. `Victory` loops Idle with a small pulse. `Defeat` switches to the
corpse texture, applies a small tilt and lowers opacity. `Death` is a Blueprint-callable alias of
the same corpse presentation.

The Ironclad profile is enabled for the player combatant by default. Enemy use is opt-in through
`bAnimateEnemyCharacter`, so an enemy using a different source image is not accidentally replaced
by Ironclad frames.

## Blueprint tuning surface

`UBattleHUDCombatantPresentationWidgetBase` exposes these properties in the existing combatant
Widget Blueprint under `Battle HUD | Combatant Animation`:

- `bEnableNativeCharacterAnimation`
- `bAnimateEnemyCharacter`
- `IdleAnimationDuration` (default `6.6666`)
- `HitAnimationDuration` (default `0.3333`)
- `AttackAnimationDuration` (default `0.30`)
- `AttackTranslation` (default `42, 0`)
- `VictoryPulseScale` (default `1.04`)
- `DefeatOpacity` (default `0.65`)
- optional `IdleAnimationFrames`, `HitAnimationFrames` and `CorpseTexture` overrides

The native defaults fill the optional frame arrays from the imported Ironclad assets. Blueprint
arrays can replace them for a different skin/profile without changing the record mapping.

## Deterministic playback rules

Frame selection is a pure elapsed-time calculation. Idle and Victory wrap with modulo; Hit and
Attack clamp to the final phase and return to Idle when their configured duration ends. The helper
`GetAnimationFrameIndex` is exposed to Blueprint and does not depend on frame rate, UObject
addresses, or Gameplay state. Every action starts at elapsed time zero, and `StopCombatantAnimation`
restores the authored Blueprint brush, transform and opacity.

## Validation and acceptance

- Bundled UE 5.8 project-file generation: PASS.
- Development Editor Win64 build: PASS after terminating the stale Live Coding/editor processes
  that held the build lock (`Saved/Logs/Log.txt` contains the normal build trace).
- `CompileAllBlueprints`: 0 errors; `WBP_CombatantPresentation` compiled successfully. The
  commandlet retained only the project's existing unrelated warnings
  (`Saved/Logs/IroncladCharacterAnimationBlueprints.log`).
- Imported texture inventory: 120 Idle `.uasset`, 8 Hit `.uasset`, 1 corpse `.uasset`.
- Native PIE visual acceptance remains a user gate. Open the Native battle map
  `/Game/SlayTheSpireDemo/Maps/L_BattleTest_Native` and
  verify that Idle loops, playing a card produces a short player lunge, damage produces the Hit
  reaction, and terminal Victory/Defeat leaves the expected final pose. Also verify that the
  existing `Img_Character` layout remains aligned and that disabling the profile restores the
  authored static texture.

The source animation set intentionally stops at the authored `Idle`/`Hit` plus the documented
presentation fallbacks. A future attack/death source can be added by replacing the optional frame
arrays; no Gameplay or record contract changes are required.
