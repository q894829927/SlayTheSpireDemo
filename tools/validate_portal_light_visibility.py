"""Disposable native PIE probe. Execute through UE Python, never saves assets.

Measures opaque ray visibility through the public TLAS hook, not Lumen lighting.
"""
import json
from pathlib import Path
import time
import traceback
import unreal as u

out = Path(u.Paths.project_saved_dir()) / 'AutomationReports/PortalLightVisibility'
out.mkdir(parents=True, exist_ok=True)
editor = u.get_editor_subsystem(u.LevelEditorSubsystem)
state = {'started': time.monotonic(), 'frame': 0, 'captures': [], 'completed': False}
settings = u.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle = settings.get_editor_property('bThrottleCPUWhenNotForeground')
settings.set_editor_property('bThrottleCPUWhenNotForeground', False)


def finish(error=None):
    state.update(completed=error is None, error=error)
    (out / 'sequence.json').write_text(json.dumps({k: v for k, v in state.items() if k not in
        ('handle', 'world', 'system', 'blocker')}, indent=2, default=str), encoding='utf-8')
    u.SystemLibrary.execute_console_command(None, 'portal.StopLightVisibilityProbe')
    settings.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    u.unregister_slate_post_tick_callback(state['handle'])
    editor.editor_request_end_play()
    u.SystemLibrary.execute_console_command(None, 'QUIT_EDITOR')


def tick(dt):
    try:
        if time.monotonic() - state['started'] > 180:
            raise RuntimeError('Native TLAS diagnostic timed out')
        if 'world' not in state:
            world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
            if not world:
                return
            systems = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)
            if not systems:
                return
            state.update(world=world, system=systems[0])
            u.GameplayStatics.set_global_time_dilation(world, .0001)
            pawn = u.GameplayStatics.get_player_pawn(world, 0)
            pawn.get_editor_property('character_movement').set_movement_mode(u.MovementMode.MOVE_FLYING)
            pawn.set_actor_location(u.Vector(1300, 700, 72.148), False, True)
            u.GameplayStatics.get_player_controller(world, 0).set_control_rotation(u.Rotator(pitch=-4.9402, yaw=90))
        index, phase = divmod(state['frame'], 100)
        if index == 3:
            finish()
            return
        name = ['baseline', 'thin_exit_blocker', 'blocker_removed_restart'][index]
        world = state['world']
        if phase == 0:
            u.SystemLibrary.execute_console_command(world, 'portal.StopLightVisibilityProbe')
            if index == 1:
                exit = state['system'].get_editor_property('orange_portal')
                location = exit.get_actor_location() + exit.get_actor_forward_vector() * .15
                # Python does not expose the Blueprint-internal deferred-spawn helper.
                # Reuse an unrelated existing PIE-only wall, guarding both supports.
                supports = [state['system'].get_editor_property(name).get_editor_property('support')
                            for name in ('blue_portal', 'orange_portal')]
                candidates = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
                              if a.static_mesh_component not in supports and a.static_mesh_component.is_visible()
                              and a.static_mesh_component.get_editor_property('visible_in_ray_tracing')]
                actor = sorted(candidates, key=lambda a: (-a.get_distance_to(exit), a.get_path_name()))[0]
                mesh = actor.static_mesh_component
                state['blocker_original_transform'] = actor.get_actor_transform()
                state['blocker_original_mesh'] = mesh.get_editor_property('static_mesh')
                state['blocker_original_collision'] = mesh.get_collision_enabled()
                mesh.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
                mesh.set_static_mesh(u.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
                mesh.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
                actor.set_actor_location(location, False, True)
                actor.set_actor_rotation(exit.get_actor_rotation(), True)
                actor.set_actor_scale3d(u.Vector(.001, .4, .4))
                state['blocker'] = actor
                state['blocker_description'] = {'name': actor.get_path_name(), 'location': str(location),
                    'rotation': str(exit.get_actor_rotation()), 'fullThicknessCm': .1,
                    'nearFaceDistanceFromLogicalPlaneCm': .1}
            if index == 2:
                mesh = state['blocker'].static_mesh_component
                mesh.set_static_mesh(state['blocker_original_mesh'])
                mesh.set_collision_enabled(state['blocker_original_collision'])
                state['blocker'].set_actor_transform(state['blocker_original_transform'], False, True)
        if phase == 30:
            u.SystemLibrary.execute_console_command(world, 'portal.StartLightVisibilityProbe')
        if phase == 90:
            u.SystemLibrary.execute_console_command(world, 'portal.DumpLightVisibilityProbe')
            report = json.loads((out.parent / 'PortalLightVisibilityProbe.json').read_text(encoding='utf-8-sig'))
            state['captures'].append({'case': name, 'report': report})
            if report['status'] != 'GPU_READBACK_COMPLETE_OPAQUE_VISIBILITY_ONLY':
                raise RuntimeError('No native GPU completion: ' + report['status'])
            for e in range(2):
                rays = report['rays'][e*4:e*4+4]
                for c in (0, 2, 3):
                    if index == 1 and e == 1 and c == 0:
                        continue  # The new blocker is correctly nearer than this wall.
                    if rays[c]['status'] != 0 or rays[c]['hitPrimitive'] != report['supportIndices'][e]:
                        raise RuntimeError('Stock/outside/back support mismatch: ' + str(rays[c]))
            if index == 1:
                ray = report['rays'][1]
                if ray['status'] != 0 or ray['hops'] != 1 or abs(ray['distanceCm'] - 50.1) > .01:
                    raise RuntimeError('Thin exit blocker lost: ' + str(ray))
                for c in (4, 5):
                    other = report['rays'][c]
                    if other['status'] != 0 or other['hops'] != 0 or other['hitPrimitive'] != ray['hitPrimitive'] or abs(other['distanceCm'] - 49.8) > .01:
                        raise RuntimeError('Opposite-side foreground blocker bypassed: ' + str(other))
            if index == 2:
                for c in (1, 5):
                    first = state['captures'][0]['report']['rays'][c]
                    last = report['rays'][c]
                    if first['status'] != last['status'] or abs(first['distanceCm']-last['distanceCm']) > .01:
                        raise RuntimeError('Restart did not restore baseline visibility')
        state['frame'] += 1
    except Exception:
        finish(traceback.format_exc())


state['handle'] = u.register_slate_post_tick_callback(tick)
editor.editor_request_begin_play()
