# Awakened One Monster Animation

Last updated: **2026-09-11**

## Scope

The Native combatant presentation widget now plays an animated Awakened One profile for the
enemy shown in the battle demo. Runtime playback remains Presentation-only: Gameplay commits
the existing `CardPlayed` and `Damage` records, `UBattleHUDWidget` maps accepted records to
animation requests, and `UBattleHUDCombatantPresentationWidgetBase` swaps imported textures on
the existing `Img_Character` surface. No Spine runtime is linked to the project.

All visual verification for this profile uses:

```text
/Game/SlayTheSpireDemo/Maps/L_Battle_RuinedCitadel
```

## Source and bake

The source Spine data is retained at:

```text
Content/SlayTheSpireDemo/UI/images/monsters/theForest/awakenedOne/
```

The source animations are baked offline to transparent 900x656 PNG canvases and imported as
ordinary `Texture2D` assets. The active profile is:

| Source animation | Imported frames | Authored/runtime duration | Runtime use |
| --- | ---: | ---: | --- |
| `Idle_2` | 48 | 2.40 s | looping breathing, tail and eye motion |
| `Hit` | 8 | 0.3333 s | one-shot damage reaction |
| `Attack_1` | 24 | 1.00 s | one-shot enemy attack |

The imported textures are stored under:

```text
/Game/SlayTheSpireDemo/UI/Textures/AwakenedOne/Idle/
/Game/SlayTheSpireDemo/UI/Textures/AwakenedOne/Hit/
/Game/SlayTheSpireDemo/UI/Textures/AwakenedOne/Attack/
```

The static texture remains available as the fallback when the animated assets cannot be
loaded:

```text
/Game/SlayTheSpireDemo/UI/Textures/T_AwakenedOne_Static
```

## Why the first result looked static

There were two independent causes. The first import pass was not saved into the project's
`Content` directory, so the soft paths resolved to the static fallback. After the assets were
saved, the first selected source animation was `Idle_1`. That animation is a near-static combat
stance; its frame differences are too small to read as movement at the in-game scale. The active
profile now uses the authored `Idle_2` animation, whose tail and eye movement are visible in the
Ruined Citadel battle.

## Record-to-animation mapping

Animation requests are issued only after the Native presentation record has passed its payload
and token validation and its finish timer has started:

| Committed record | Presentation id | Animation |
| --- | --- | --- |
| Player `CardPlayed` for an Attack card | `SourcePresentationId` | player Attack presentation |
| `Damage` received by the enemy | `TargetPresentationId` | enemy Hit |
| `Damage` received by the player from a non-player Attack source | enemy source id | enemy `Attack_1` |
| Terminal Victory / Defeat | winner / defeated id | Victory pulse / explicit corpse texture, otherwise the last valid animation frame |

Enemy `Attack_1` is therefore tied to the same committed damage boundary that changes the
player's HP. It does not add a Gameplay action, change damage ordering or infer intent from a
frame timer. If the source id is unavailable, the existing record validation rejects the visual
request rather than guessing a target.

## Blueprint controls

The existing `WBP_CombatantPresentation` parent exposes the profile controls under
`Battle HUD | Combatant Animation`:

- `bEnableNativeCharacterAnimation`
- `bAnimateEnemyCharacter` (enabled for the placed enemy instance)
- `EnemyIdleAnimationDuration` (default `2.40`)
- `EnemyHitAnimationDuration` (default `0.3333`)
- `EnemyAttackAnimationDuration` (default `1.0`)
- optional `EnemyIdleAnimationFrames`, `EnemyHitAnimationFrames`,
  `EnemyAttackAnimationFrames` and `EnemyCorpseTexture` overrides

The native defaults fill empty enemy arrays from the Awakened One asset paths. A Blueprint can
replace those arrays for another monster profile without changing Gameplay or the presentation
record schema. Disabling `bAnimateEnemyCharacter` restores the authored static brush behavior.

When the enemy has no `EnemyCorpseTexture` override, entering the terminal Defeat animation
does not replace the animated enemy with the Blueprint's original static brush. The widget
keeps the last valid runtime frame; if the transition arrives before a frame was displayed,
it uses the last valid Idle frame. An explicit corpse texture still takes priority.

## Deterministic playback rules

Frame selection is based on elapsed time and the configured duration. Idle wraps with modulo;
Hit and Attack clamp to their one-shot duration. Enemy Attack uses its authored frame sequence
directly and returns to Idle after one second. The texture canvas is shared across all three
sequences, so swapping a frame does not introduce a position jump.

Loaded animation textures are retained by the presentation widget while it is alive. If a soft
frame reference is invalidated, the widget synchronously reloads that frame before applying it;
an unavailable enemy frame falls back to the last valid animated frame or Idle frame, not the
static brush.

## Validation

- Bundled UE 5.8 project-file generation: **PASS** (`Saved/Logs/AwakenedOneMonsterAnimationFinalProjectFiles.log`).
- Development Editor Win64 build: **PASS** (`Saved/Logs/AwakenedOneMonsterAnimationFinalBuild.log`).
- MCP `WBP_CombatantPresentation` compile/save: **PASS**.
- Imported active runtime inventory: **48 Idle_2 + 8 Hit + 24 Attack_1** textures, plus the static fallback.
- Focused floating PIE on `L_Battle_RuinedCitadel`: **PASS for visible Idle_2 playback**; two captures
  separated by 1.2 seconds show different tail poses and eye state while the enemy remains in place.
- Shipping Win64 compile and BuildCookRun packaging after the terminal defeat fix: **PASS**;
  final output archived at `E:\UE_DEMO\Packed_DeathTextureFix_v2\Windows`.

**USER ACTION REQUIRED:** for the final visual gate, open `L_Battle_RuinedCitadel`, end the
player turn, and observe the enemy attack pose while the player takes damage. Then play an Attack
card into the enemy and confirm the enemy Hit pose. The expected result is a visible `Attack_1`
sequence on the enemy turn, a short `Hit` sequence on player damage, and continuous Idle_2 motion
between records. When the enemy is defeated, it should keep its last valid animated frame (or an
explicit corpse override) at the configured defeat opacity instead of reverting to
`T_AwakenedOne_Static`.

## Layout amendment — 2026-09-11

Through Unreal MCP, `WBP_BattleHUD_Native` now groups `Combatant_EnemyPresentation` and
`EnemyIntentPanel` under the saved `EnemyCombatantCluster` Overlay. The cluster keeps the
original enemy Canvas anchor and `650 × 350` presentation area; the intent panel is centered
inside that area with a top padding of `55`, so the icon and damage number follow the monster
instead of using an unrelated screen anchor.

The enemy presentation instance uses a `(1.18, 1.18)` Render Transform scale with a bottom-center
pivot `(0.5, 1.0)`. The shared combatant template remains at scale `1.0`, so the player is not
resized. MCP compile/save and focused PIE on `L_Battle_RuinedCitadel` confirmed the larger enemy,
unchanged player size and the intent positioned directly above the enemy.

The unused legacy `Img_PlayerCharacter` and `Img_EnemyCharacter` widgets were also removed from
`WBP_BattleHUD_Native`. They had no C++ bindings, but UMG Designer could still draw a selected
collapsed brush beside the Native presentation, which made the editor preview look like two
overlapping character images.
