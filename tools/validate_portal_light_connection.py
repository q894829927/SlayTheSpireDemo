"""Native D3D12 PIE readback of shared Scene UB publication; never saves assets.

This validates data and opaque visibility, not Lumen radiance or continuous visuals.
"""
import json
from pathlib import Path
import time
import traceback
import unreal as u

out = Path(u.Paths.project_saved_dir()) / 'AutomationReports/PortalLightConnection'
out.mkdir(parents=True, exist_ok=True)
editor = u.get_editor_subsystem(u.LevelEditorSubsystem)
settings = u.load_object(None, '/Script/UnrealEd.Default__EditorPerformanceSettings')
old_throttle = settings.get_editor_property('bThrottleCPUWhenNotForeground')
settings.set_editor_property('bThrottleCPUWhenNotForeground', False)
cases = ['baseline', 'offscreen', 'endpoint_moved', 'support_replaced', 'restored',
         'thin_exit_blocker', 'cleared', 'relinked', 'recursive_depth4', 'observer_restarted']
state = {'started': time.monotonic(), 'frame': 0, 'captures': [], 'completed': False}


def command(text):
    u.SystemLibrary.execute_console_command(state.get('world'), text)


def finish(error=None):
    (out / 'sequence.json').write_text(json.dumps({'completed': error is None, 'error': error,
        'captures': state['captures'], 'elapsedSeconds': time.monotonic()-state['started']},
        indent=2), encoding='utf-8')
    command('portal.StopLightVisibilityProbe')
    settings.set_editor_property('bThrottleCPUWhenNotForeground', old_throttle)
    u.unregister_slate_post_tick_callback(state['handle'])
    editor.editor_request_end_play()
    command('QUIT_EDITOR')


def assert_report(name, report):
    if report['status'] != 'GPU_READBACK_COMPLETE_OPAQUE_VISIBILITY_ONLY':
        raise AssertionError('No completed GPU frame: ' + report['status'])
    views = report['views']
    main = [v for v in views if v['main']]
    assert len(main) == 1, (name, 'must observe exactly one player main view', views)
    canonical = main[0]
    for view in views:
        for field in ('generation', 'endpointCount', 'backendABI', 'sceneSession', 'sealedFrame',
                      'supportIndices', 'surfaceIndices'):
            assert view[field] == canonical[field], (name, 'mixed scene publication', field, views)
        assert int(view['sealedFrame']) == int(report['frame']), (name, 'stale frame', view)
        for first, other in zip(canonical['rays'], view['rays']):
            for field in ('status', 'hitPrimitive', 'hops'):
                assert first[field] == other[field], (name, 'TLAS basis/visibility mismatch', first, other)
            assert abs(first['distanceCm'] - other['distanceCm']) < .01, (name, first, other)
    expected_count = 0 if name == 'cleared' else 2
    assert canonical['endpointCount'] == expected_count, (name, canonical)
    assert canonical['backendABI'] == 1 and int(canonical['sceneSession']) > 0
    if name in ('baseline', 'restored', 'relinked', 'recursive_depth4', 'observer_restarted'):
        assert len(views) >= (5 if name in ('recursive_depth4', 'observer_restarted') else 2), (name, 'missing auxiliary views', len(views))
    if name == 'offscreen':
        assert len(views) == 1, ('offscreen producer still submitted auxiliary views', len(views))
        assert canonical['generation'] == state['captures'][0]['report']['views'][0]['generation']
    if name == 'cleared':
        assert all(r['status'] == 3 and r['hops'] == 0 for r in canonical['rays'])
    elif name != 'support_replaced':
        for e in range(2):
            for c in (0, 2, 3):
                if name == 'thin_exit_blocker' and e == 1 and c == 0:
                    continue
                ray = canonical['rays'][e*4+c]
                assert ray['status'] == 0 and ray['hitPrimitive'] == canonical['supportIndices'][e], (name, ray, canonical)
        if name == 'thin_exit_blocker':
            ray = canonical['rays'][1]
            assert ray['status'] == 0 and ray['hops'] == 1 and abs(ray['distanceCm']-50.1) < .01, ray
        else:
            for c in (1, 5):
                ray = canonical['rays'][c]
                assert ray['status'] == 1 and ray['hops'] == 1 and abs(ray['distanceCm']-100) < .01, (name, ray)
    if state['captures']:
        previous = next(v for v in state['captures'][-1]['report']['views'] if v['main'])
        # Ordinary opaque geometry changes visibility through the freshly built TLAS;
        # it is not a portal-topology mutation and must not advance this generation.
        if name not in ('offscreen', 'thin_exit_blocker', 'observer_restarted'):
            assert int(canonical['generation']) > int(previous['generation']), (name, 'generation did not invalidate', previous, canonical)
        else:
            assert canonical['generation'] == previous['generation']
        assert canonical['sceneSession'] == previous['sceneSession'], ('observer owns scene lifetime', name)
        assert int(canonical['sealedFrame']) > int(previous['sealedFrame'])


def tick(dt):
    try:
        if time.monotonic() - state['started'] > 600:
            raise RuntimeError('Scene connection diagnostic timed out')
        if 'world' not in state:
            world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
            if not world:
                return
            systems = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)
            if len(systems) != 1:
                return
            s = systems[0]
            blue, orange = (s.get_editor_property(n) for n in ('blue_portal', 'orange_portal'))
            pawn = u.GameplayStatics.get_player_pawn(world, 0)
            pc = u.GameplayStatics.get_player_controller(world, 0)
            pawn.get_editor_property('character_movement').set_movement_mode(u.MovementMode.MOVE_FLYING)
            pawn.set_actor_location(u.Vector(1300, 700, 72.148), False, True)
            pc.set_control_rotation(u.Rotator(pitch=-4.9402, yaw=90))
            u.GameplayStatics.set_global_time_dilation(world, .0001)
            support = orange.get_editor_property('support')
            candidates = [a for a in u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
                          if a.static_mesh_component not in (blue.get_editor_property('support'), support)
                          and a.static_mesh_component.is_visible()
                          and a.static_mesh_component.get_editor_property('visible_in_ray_tracing')]
            fixture = sorted(candidates, key=lambda a: (-a.get_distance_to(orange), a.get_path_name()))[0]
            host = support.get_owner()
            state.update(world=world, system=s, blue=blue, orange=orange, pc=pc, fixture=fixture,
                original_exit=orange.get_actor_transform(), original_support=support,
                fixture_transform=fixture.get_actor_transform(), fixture_mesh=fixture.static_mesh_component.static_mesh,
                fixture_collision=fixture.static_mesh_component.get_collision_enabled(),
                host=host, host_transform=host.get_actor_transform())
        index, phase = divmod(state['frame'], 110)
        if index == len(cases):
            finish()
            return
        name = cases[index]
        s, orange, fixture = state['system'], state['orange'], state['fixture']
        mesh = fixture.static_mesh_component
        if phase == 0:
            if name == 'offscreen':
                state['pc'].set_control_rotation(u.Rotator(yaw=-90))
            if name == 'endpoint_moved':
                state['pc'].set_control_rotation(u.Rotator(pitch=-4.9402, yaw=90))
                orange.set_actor_location(orange.get_actor_location() + orange.get_actor_right_vector() * .1, False, True)
            if name == 'support_replaced':
                orange.set_editor_property('support', mesh)
            if name == 'restored':
                orange.set_actor_transform(state['original_exit'], False, True)
                orange.set_editor_property('support', state['original_support'])
            if name == 'thin_exit_blocker':
                mesh.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
                mesh.set_static_mesh(u.load_object(None, '/Engine/BasicShapes/Cube.Cube'))
                mesh.set_collision_enabled(u.CollisionEnabled.NO_COLLISION)
                fixture.set_actor_location(orange.get_actor_location()+orange.get_actor_forward_vector()*.15, False, True)
                fixture.set_actor_rotation(orange.get_actor_rotation(), True)
                fixture.set_actor_scale3d(u.Vector(.001, .4, .4))
            if name == 'cleared':
                mesh.set_static_mesh(state['fixture_mesh'])
                mesh.set_collision_enabled(state['fixture_collision'])
                fixture.set_actor_transform(state['fixture_transform'], False, True)
                s.reset_portals()
            if name == 'relinked':
                state['blue'].set_editor_property('bPlaced', True)
                orange.set_editor_property('bPlaced', True)
            if name == 'recursive_depth4':
                host = state['host']
                host.static_mesh_component.set_editor_property('mobility', u.ComponentMobility.MOVABLE)
                host.set_actor_location(u.Vector(1300,444,120), False, True)
                host.set_actor_rotation(u.Rotator(), True)
                host.set_actor_scale3d(u.Vector(2.2,.12,3))
                orange.set_actor_location(u.Vector(1300,450,105), False, True)
                orange.set_actor_rotation(u.Rotator(yaw=90), True)
                s.set_editor_property('recursion_depth', 4)
            if name == 'observer_restarted':
                command('portal.StopLightVisibilityProbe')
        if phase == 35:
            command('portal.StartLightVisibilityProbe' if index == 0 or name == 'observer_restarted'
                    else 'portal.CaptureLightVisibilityProbe')
        if phase == 100:
            command('portal.DumpLightVisibilityProbe')
            report = json.loads((out.parent/'PortalLightVisibilityProbe.json').read_text(encoding='utf-8-sig'))
            assert_report(name, report)
            state['captures'].append({'case': name, 'report': report})
        state['frame'] += 1
    except Exception:
        finish(traceback.format_exc())

state['handle'] = u.register_slate_post_tick_callback(tick)
editor.editor_request_begin_play()
