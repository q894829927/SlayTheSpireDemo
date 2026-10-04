"""Run in L_Interior_LivingKitchen PIE through the UE Python plugin.

Exercises actual CharacterMovement input at 20 cm/s: stop inside either entry,
resume through it, stop at the exit, retreat before crossing, and strafe into the
rim before retreating. Writes Saved/PortalPlayerWallStuck.json. No assets saved.
Stop PIE only after the report completes (or call unreal.portal_wall_stuck_stop()).
"""
import json
import os
import traceback
import unreal as u

if hasattr(u, 'portal_wall_stuck_stop'):
    u.portal_wall_stuck_stop('Superseded by a new replay')

world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world, 'Start L_Interior_LivingKitchen PIE first'
system = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)[0]
player = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_pawn(world, 0)
movement = pawn.character_movement
blue = system.get_editor_property('blue_portal')
orange = system.get_editor_property('orange_portal')
cases = [(gate, kind) for kind in ('cross_pause', 'retreat', 'rim_retreat')
         for gate in (blue, orange)]
original_speed = movement.max_walk_speed
floor_center = pawn.get_actor_location().z
movement.max_walk_speed = 20
player.set_ignore_move_input(True)
player.set_ignore_look_input(True)
path = u.Paths.project_saved_dir() + 'PortalPlayerWallStuck.json'
report = {'passed': False, 'completed': 0, 'cases': [], 'error': None}
state = {'done': False, 'index': -1}


def write():
    with open(path + '.tmp', 'w') as stream:
        json.dump(report, stream, indent=2)
    os.replace(path + '.tmp', path)


def stop(error=None):
    if state['done']:
        return
    state['done'] = True
    movement.stop_movement_immediately()
    movement.max_walk_speed = original_speed
    player.set_ignore_move_input(False)
    player.set_ignore_look_input(False)
    u.unregister_slate_post_tick_callback(state['handle'])
    report.update(passed=error is None, error=error)
    write()


def phase(name, now):
    state.update(stage=name, since=now)


def begin(now):
    state['index'] += 1
    gate, kind = cases[state['index']]
    center = gate.get_actor_location() + gate.get_actor_forward_vector() * 55
    center.z = floor_center
    movement.stop_movement_immediately()
    pawn.set_actor_location(center, False, True)
    state.update(entry=gate, kind=kind, crossed=False,
                 start_count=system.get_editor_property('player_crossings'))
    report['cases'].append({'entry': 'blue' if gate == blue else 'orange',
                            'kind': kind, 'passed': False, 'samples': []})
    phase('settle', now)


def verify_corrected_footprint(side):
    # UE Python unary minus mutates a Vector. Preserve the basis as scalar tuples
    # because this probe reuses it across several movement requests and assertions.
    forward = blue.get_actor_forward_vector()
    right = blue.get_actor_right_vector()
    normal = (forward.x, forward.y, forward.z)
    tangent = (right.x, right.y, right.z)

    def delta(x, y):
        return u.Vector(*(normal[i] * x + tangent[i] * y for i in range(3)))

    def position():
        value = pawn.get_actor_location()
        return (value.x, value.y, value.z)

    def move(x, y):
        movement.move_updated_component(delta(x, y), pawn.get_actor_rotation(), True)

    movement.stop_movement_immediately()
    start = blue.get_actor_location() + delta(65, 0)
    start.z = floor_center
    pawn.set_actor_location(start, False, True)
    move(0, 0)
    move(-55, 0)
    move(0, side * 300)
    rim = position()
    pawn.set_actor_location(u.Vector(*(rim[i] + tangent[i] * side * .25 for i in range(3))), False, True)
    before = position()
    move(8, 0)
    after = position()
    move(-8, side)
    blocked = position()
    move(0, -side * 10)
    centered = position()
    move(50, 0)
    result = {'side': side,
              'retreated_cm': sum((after[i] - before[i]) * normal[i] for i in range(3)),
              'deeper_wall_move_cm': sum((blocked[i] - after[i]) ** 2 for i in range(3)) ** .5,
              'toward_center_cm': sum((blocked[i] - centered[i]) * tangent[i] * side for i in range(3)),
              'final_state': str(system.get_editor_property('player_crossing_state'))}
    report.setdefault('footprint_recovery', []).append(result)
    assert abs(result['retreated_cm'] - 8) < .01, 'Corrected footprint cannot retreat'
    assert result['deeper_wall_move_cm'] < .01, 'Invalid footprint entered deeper into wall'
    assert abs(result['toward_center_cm'] - 10) < .01, 'Corrected footprint cannot approach center'
    assert 'OUTSIDE' in result['final_state'], 'Corrected footprint retained passage state'


def step(dt):
    if state['done']:
        return
    try:
        now = u.GameplayStatics.get_time_seconds(world)
        if state['index'] < 0:
            begin(now)
        count = system.get_editor_property('player_crossings') - state['start_count']
        assert count in (0, 1), 'Unexpected multiple crossings'
        assert state['kind'] == 'cross_pause' or count == 0, 'Retreat crossed the entry'
        entry = state['entry']
        gate = (orange if entry == blue else blue) if count else entry
        normal = gate.get_actor_forward_vector()
        position = pawn.get_actor_location()
        distance = (position - gate.get_actor_location()).dot(normal)
        crossing_state = str(system.get_editor_property('player_crossing_state'))
        current = report['cases'][-1]
        current['samples'].append({'stage': state['stage'], 'distance': distance,
                                   'location': [position.x, position.y, position.z],
                                   'count': count, 'state': crossing_state,
                                   'delta_seconds': u.GameplayStatics.get_world_delta_seconds(world)})
        assert distance >= -.5, 'Capsule escaped behind support'
        assert abs(position.z - floor_center) < 50, 'Unexpected floor loss or step launch'
        if count and not state['crossed']:
            state['crossed'] = True
            movement.stop_movement_immediately()
            phase('pause_exit', now)
        elapsed = now - state['since']
        stage = state['stage']
        if stage == 'settle':
            if elapsed > .3:
                phase('approach', now)
        elif stage == 'approach':
            if distance <= 10:
                movement.stop_movement_immediately()
                phase('rim' if state['kind'] == 'rim_retreat' else 'pause_entry', now)
            else:
                pawn.add_movement_input(-normal, 1, True)
        elif stage == 'rim':
            pawn.add_movement_input(gate.get_actor_right_vector(), 1, True)
            if elapsed > 1.5:
                movement.stop_movement_immediately()
                phase('pause_entry', now)
        elif stage == 'pause_entry':
            if elapsed > 1:
                phase('through' if state['kind'] == 'cross_pause' else 'out', now)
        elif stage == 'through':
            pawn.add_movement_input(-normal, 1, True)
        elif stage == 'pause_exit':
            if elapsed > 1:
                phase('out', now)
        elif stage == 'out':
            if distance >= 50:
                assert 'OUTSIDE' in crossing_state, 'Passage collision state stayed active'
                current['passed'] = True
                report['completed'] += 1
                write()
                if report['completed'] == len(cases):
                    verify_corrected_footprint(1)
                    verify_corrected_footprint(-1)
                    stop()
                    return
                begin(now)
            else:
                pawn.add_movement_input(normal, 1, True)
        assert elapsed < 6, 'Movement stuck in ' + stage
        if len(current['samples']) % 60 == 0:
            write()
    except Exception:
        stop(traceback.format_exc())


u.portal_wall_stuck_stop = stop
state['handle'] = u.register_slate_post_tick_callback(step)
write()
