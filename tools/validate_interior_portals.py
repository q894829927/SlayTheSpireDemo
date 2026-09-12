"""Run via UE Python during PIE in L_Interior_LivingKitchen. Never saves the test world.

Exercises real CharacterMovement and Chaos, reports to Saved/PortalRuntimeValidation.json.
Stops at the first failed scenario. This is state evidence, not visual acceptance.
"""
import unreal as u
import json
import traceback

world = u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE first'
system = u.GameplayStatics.get_all_actors_of_class(world, u.InteriorPortalSystem)[0]
player = u.GameplayStatics.get_player_controller(world, 0)
pawn = u.GameplayStatics.get_player_pawn(world, 0)
body = system.get_editor_property('physics_travellers')[0]
blue = system.get_editor_property('blue_portal')
orange = system.get_editor_property('orange_portal')
original_body = body.get_world_transform()
evidence = []
state = {'stage': 0, 'start': u.GameplayStatics.get_time_seconds(world)}

def vec(v): return [v.x, v.y, v.z]
def reset_player(pos, yaw):
    pawn.character_movement.stop_movement_immediately()
    pawn.set_actor_location(pos, False, True)
    player.set_control_rotation(u.Rotator(yaw=yaw))

def begin(stage):
    state.update(stage=stage, start=u.GameplayStatics.get_time_seconds(world), launched=False)
    state['before'] = system.get_editor_property('physics_crossings' if stage == 2 else 'player_crossings')
    if stage == 0: reset_player(u.Vector(1300, 750, 72.15), 90)
    if stage == 1: reset_player(u.Vector(1600, 1200, 72.15), 0)
    if stage == 2:
        reset_player(u.Vector(1300, 300, 72.15), 90)
        body.set_enable_gravity(False)
        body.set_world_location(u.Vector(1300, 740, 105), False, True)
        body.set_physics_linear_velocity(u.Vector())
        body.set_physics_angular_velocity_in_radians(u.Vector())

def finish(error=None):
    u.unregister_slate_post_tick_callback(u.portal_validation_handle)
    pawn.character_movement.stop_movement_immediately()
    body.set_world_transform(original_body, False, True)
    body.set_enable_gravity(True)
    body.set_physics_linear_velocity(u.Vector())
    report = {'passed': error is None, 'scenarios': evidence, 'error': error}
    with open(u.Paths.project_saved_dir()+'PortalRuntimeValidation.json', 'w') as f:
        json.dump(report, f, indent=2)
    u.log('PORTAL_RUNTIME_VALIDATION '+json.dumps(report))

def step(dt):
    try:
        elapsed = u.GameplayStatics.get_time_seconds(world) - state['start']
        stage = state['stage']
        if elapsed < .3: return
        if stage < 2:
            pawn.add_movement_input(u.Vector(0,1,0) if stage == 0 else u.Vector(1,0,0), 1, True)
            count = system.get_editor_property('player_crossings')
            if count > state['before']:
                velocity = pawn.get_velocity()
                assert abs(velocity.length()-260) < 2, str(velocity)
                assert velocity.x < -250 if stage == 0 else velocity.y < -250, str(velocity)
                evidence.append({'scenario': 'blue_to_orange' if stage == 0 else 'orange_to_blue', 'crossings':count-state['before'], 'position':vec(pawn.get_actor_location()), 'velocity':vec(velocity)})
                begin(stage+1)
        else:
            if not state['launched']:
                body.set_physics_linear_velocity(u.Vector(0,700,0))
                state['launched'] = True
            count = system.get_editor_property('physics_crossings')
            if count > state['before']:
                velocity = body.get_physics_linear_velocity()
                assert velocity.x < -600, str(velocity)
                evidence.append({'scenario':'physics_cube', 'crossings':count-state['before'], 'position':vec(body.get_world_location()), 'velocity':vec(velocity)})
                finish()
                return
        assert elapsed < 5, 'Timed out at stage '+str(stage)+'; pawn='+str(pawn.get_actor_location())+'; cube='+str(body.get_world_location())+'; message='+system.get_editor_property('placement_message')
    except Exception:
        finish(traceback.format_exc())

begin(0)
u.portal_validation_handle = u.register_slate_post_tick_callback(step)
