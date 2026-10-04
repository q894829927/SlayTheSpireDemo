"""Verify the player slice and remote flashlight path in L_Interior_LivingKitchen PIE.

Run after starting PIE. The script moves the player to the authored blue portal
entry, enables the existing flashlight for one second, records the presentation
component's active remote counts, restores the original pose/light state and
writes Saved/PortalPlayerPresentationRuntime.json. It does not save the test world.
"""
import json
import os
import traceback
import unreal as u


world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world, 'Start L_Interior_LivingKitchen PIE first'
system = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)[0]
player = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_pawn(world, 0)
blue = system.get_editor_property('blue_portal')
presentation = system.get_editor_property('player_presentation')
flashlight = pawn.get_editor_property('flashlight')
movement = pawn.get_editor_property('character_movement')

original_transform = pawn.get_actor_transform()
original_light_visible = flashlight.is_visible()
original_light_intensity = flashlight.get_editor_property('intensity')
original_speed = movement.get_editor_property('max_walk_speed')
path = u.Paths.project_saved_dir() + 'PortalPlayerPresentationRuntime.json'
state = {'done': False, 'start': u.GameplayStatics.get_time_seconds(world)}
report = {
    'passed': False,
    'error': None,
    'map': '/Game/House/L_Interior_LivingKitchen',
    'slice_material_count': None,
    'active_remote_visuals': None,
    'active_remote_lights': None,
    'crossing_state': None,
}


def write():
    with open(path + '.tmp', 'w') as stream:
        json.dump(report, stream, indent=2)
    os.replace(path + '.tmp', path)


def finish(error=None):
    if state['done']:
        return
    state['done'] = True
    try:
        pawn.set_actor_transform(original_transform, False, False)
        flashlight.set_visibility(original_light_visible)
        flashlight.set_editor_property('intensity', original_light_intensity)
        movement.set_editor_property('max_walk_speed', original_speed)
    finally:
        u.unregister_slate_post_tick_callback(state['handle'])
        report['passed'] = error is None and report['active_remote_visuals'] > 0 and report['active_remote_lights'] > 0
        report['error'] = error
        write()


def step(_delta):
    try:
        if u.GameplayStatics.get_time_seconds(world) - state['start'] < 0.75:
            return
        report['slice_material_count'] = len(presentation.get_editor_property('slice_materials'))
        report['active_remote_visuals'] = presentation.get_editor_property('active_remote_visuals')
        report['active_remote_lights'] = presentation.get_editor_property('active_remote_lights')
        report['crossing_state'] = str(system.get_editor_property('player_crossing_state'))
        finish()
    except Exception:
        finish(traceback.format_exc())


entry = blue.get_actor_location() + blue.get_actor_forward_vector() * 30
entry.z = original_transform.translation.z
pawn.set_actor_location(entry, False, True)
pawn.set_actor_rotation(u.Rotator(yaw=blue.get_actor_rotation().yaw + 180.0), False)
player.set_control_rotation(pawn.get_actor_rotation())
flashlight.set_visibility(True)
state['handle'] = u.register_slate_post_tick_callback(step)
write()
