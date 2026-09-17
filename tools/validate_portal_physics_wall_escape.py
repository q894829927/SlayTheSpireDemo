"""Exercise the authored Portal_TestCube through an entry/reverse sequence in PIE.

This targets the reported case where a rigid body crosses the opening, reverses
near the destination portal, and then slips through the support wall without a
second portal crossing. The body is still the real Chaos component, but the
replay advances it with small TeleportPhysics steps. That makes the acceptance
independent of solver sleep/contact warm-up while preserving the same
system-Tick -> controller traversal ordering that the game uses. It writes
Saved/PortalPhysicsWallEscape.json without saving the test world.
"""
import json
import os
import traceback
import unreal as u

world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world, 'Start L_Interior_LivingKitchen PIE first'
system = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)[0]
player = u.GameplayStatics.get_player_controller(world, 0)
blue = system.get_editor_property('blue_portal')
orange = system.get_editor_property('orange_portal')
actors = u.GameplayStatics.get_all_actors_of_class(world, u.StaticMeshActor)
cube_actor = next((a for a in actors if 'Portal_TestCube' in a.get_name() or 'Portal_TestCube' in a.get_actor_label()), None)
assert cube_actor, 'Portal_TestCube was not found in the PIE world'
body = cube_actor.static_mesh_component
assert body.is_simulating_physics(), 'Portal_TestCube is not simulating physics'
body.set_enable_gravity(False)

path = u.Paths.project_saved_dir() + 'PortalPhysicsWallEscape.json'
report = {
    'passed': False,
    'error': None,
    'physics_crossings_start': system.get_editor_property('physics_crossings'),
    'physics_crossings_end': None,
    'samples': [],
    'checks': {},
}
state = {'done': False, 'stage': 'settle', 'since': u.GameplayStatics.get_time_seconds(world), 'start_crossings': report['physics_crossings_start']}
cube_radius = 30.0
step_distance = 8.0


def write():
    with open(path + '.tmp', 'w') as stream:
        json.dump(report, stream, indent=2)
    os.replace(path + '.tmp', path)


def vec_scaled(value, scale):
    return u.Vector(value.x * scale, value.y * scale, value.z * scale)


def set_velocity(value):
    body.set_physics_linear_velocity(value)
    body.set_physics_angular_velocity_in_radians(u.Vector(0, 0, 0))


def advance(direction):
    """Move one deterministic step; the next game frame resolves traversal."""
    location = body.get_world_location()
    next_location = location + vec_scaled(direction, step_distance)
    body.set_world_location(next_location, False, True)


def signed_distance(portal, location):
    return (location - portal.get_actor_location()).dot(portal.get_actor_forward_vector())


def begin():
    start = blue.get_actor_location() + vec_scaled(blue.get_actor_forward_vector(), 115.0)
    start.z = 105.0
    body.set_world_location(start, False, True)
    set_velocity(u.Vector(0, 0, 0))
    state.update(stage='settle', since=u.GameplayStatics.get_time_seconds(world))


def finish(error=None):
    if state['done']:
        return
    state['done'] = True
    try:
        set_velocity(u.Vector(0, 0, 0))
        body.set_enable_gravity(True)
    finally:
        u.unregister_slate_post_tick_callback(state['handle'])
        report['physics_crossings_end'] = system.get_editor_property('physics_crossings')
        report['passed'] = error is None
        report['error'] = error
        write()


def step(_delta):
    if state['done']:
        return
    try:
        now = u.GameplayStatics.get_time_seconds(world)
        location = body.get_world_location()
        crossings = system.get_editor_property('physics_crossings') - state['start_crossings']
        blue_distance = signed_distance(blue, location)
        orange_distance = signed_distance(orange, location)
        if not report['samples'] or report['samples'][-1]['stage'] != state['stage'] or len(report['samples']) % 4 == 0:
            report['samples'].append({
                'stage': state['stage'],
                'crossings': crossings,
                'location': [location.x, location.y, location.z],
                'blue_distance': blue_distance,
                'orange_distance': orange_distance,
            })

        if state['stage'] == 'settle':
            if now - state['since'] > .35:
                state.update(stage='enter', since=now)
        elif state['stage'] == 'enter':
            # A body which has not committed a transfer must never pass the
            # back side of its entry support.
            if crossings == 0 and blue_distance < -cube_radius:
                raise AssertionError('Cube passed behind the entry support before transfer')
            if crossings >= 1:
                set_velocity(u.Vector(0, 0, 0))
                state.update(stage='pause_destination', since=now)
            else:
                advance(vec_scaled(blue.get_actor_forward_vector(), -1.0))
        elif state['stage'] == 'pause_destination':
            if now - state['since'] > .25:
                state.update(stage='reverse', since=now)
        elif state['stage'] == 'reverse':
            # The reported failure is precisely a reverse move which reaches
            # the wall's back side while the second crossing is still absent.
            if crossings == 1 and orange_distance < -cube_radius:
                raise AssertionError('Cube slipped behind the destination support while reversing')
            if crossings >= 2:
                state.update(stage='clear_entry', since=now)
            else:
                advance(vec_scaled(orange.get_actor_forward_vector(), -1.0))
        elif state['stage'] == 'clear_entry':
            if crossings >= 2 and blue_distance > 80.0:
                report['checks'] = {
                    'entry_transfer_once': crossings >= 1,
                    'reverse_transfer_once': crossings >= 2,
                    'cleared_entry_front': blue_distance > 80.0,
                    'final_blue_distance': blue_distance,
                    'final_orange_distance': orange_distance,
                }
                finish()
                return
            advance(blue.get_actor_forward_vector())

        if now - state['since'] > 6.0:
            raise AssertionError('Physics wall-escape replay stuck in ' + state['stage'])
        if len(report['samples']) % 30 == 0:
            write()
    except Exception:
        finish(traceback.format_exc())


begin()
state['handle'] = u.register_slate_post_tick_callback(step)
write()
