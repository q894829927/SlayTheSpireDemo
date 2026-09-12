"""Actual-map PIE regression: alternate the authored portals 100 times, reversing before clearance.

Execute through the UE Python plugin after starting L_Interior_LivingKitchen PIE.
Uses real CharacterMovement input, keeps authored portals/materials/physics unchanged,
and writes incremental evidence to Saved/PortalPlayerReversal.json. This proves state,
not the full manual camera/lighting acceptance matrix. Stop PIE to discard player movement.
"""
import unreal as u
import json
import traceback

world=u.get_editor_subsystem(u.UnrealEditorSubsystem).get_game_world()
assert world, 'Start PIE in L_Interior_LivingKitchen'
system=u.GameplayStatics.get_all_actors_of_class(world,u.InteriorPortalSystem)[0]
player=u.GameplayStatics.get_player_controller(world,0)
pawn=u.GameplayStatics.get_player_pawn(world,0)
blue=system.get_editor_property('blue_portal')
orange=system.get_editor_property('orange_portal')
entry=blue
initial_count=system.get_editor_property('player_crossings')
target_count=int(globals().get('PORTAL_REVERSAL_TARGET',100))
test_speed=float(globals().get('PORTAL_REVERSAL_SPEED',260))
report={'passed':False,'target':target_count,'configured_speed':test_speed,'completed':0,'reversal_checks':0,'samples':[],'error':None}
state={'stage':'settle','since':u.GameplayStatics.get_time_seconds(world),'last_count':initial_count}
original_speed=pawn.character_movement.max_walk_speed
pawn.character_movement.max_walk_speed=test_speed
original_location=pawn.get_actor_location()
# Physical keyboard state can survive PIE focus changes; the replay supplies its own forced input.
player.set_ignore_move_input(True)
player.set_ignore_look_input(True)
start=blue.get_actor_location()+blue.get_actor_forward_vector()*100
start.z=original_location.z
pawn.character_movement.stop_movement_immediately()
pawn.set_actor_location(start,False,True)

def write():
    with open(u.Paths.project_saved_dir()+'PortalPlayerReversal.json','w') as f: json.dump(report,f,indent=2)

def stop(error=None):
    u.unregister_slate_post_tick_callback(u.portal_reversal_handle)
    pawn.character_movement.stop_movement_immediately()
    pawn.character_movement.max_walk_speed=original_speed
    player.set_ignore_move_input(False)
    player.set_ignore_look_input(False)
    report.update(passed=error is None,error=error)
    write()
    u.log('PORTAL_REVERSAL_RESULT '+json.dumps(report))

def phase(name): state.update(stage=name,since=u.GameplayStatics.get_time_seconds(world))

def step(dt):
    global entry
    try:
        now=u.GameplayStatics.get_time_seconds(world)
        elapsed=now-state['since']
        if state['stage']=='settle':
            if elapsed>.4: phase('enter')
            return
        normal=entry.get_actor_forward_vector()
        position=pawn.get_actor_location()
        distance=(position-entry.get_actor_location()).dot(normal)
        count=system.get_editor_property('player_crossings')
        if state['stage']=='reverse' and 'OUTSIDE' in str(system.get_editor_property('player_crossing_state')):
            # Momentum can carry the whole capsule clear before input reverses its velocity.
            # Returning after actual clearance is a legal new traversal.
            phase('enter');elapsed=0
        if count!=state['last_count']:
            assert state['stage']=='enter', 'Unexpected transfer while '+state['stage']
            assert count==state['last_count']+1, 'Multiple transfer commits in one observed interval'
            entry=orange if entry==blue else blue
            normal=entry.get_actor_forward_vector()
            distance=(position-entry.get_actor_location()).dot(normal)
            assert distance>=-.5, 'Transferred behind exit support'
            report['completed']+=1
            report['samples'].append({'crossing':report['completed'],'exit_distance':distance,'speed':pawn.get_velocity().length(),'delta_seconds':u.GameplayStatics.get_world_delta_seconds(world)})
            state['last_count']=count
            phase('reverse' if distance<24 else 'clear')
            elapsed=0
            write()
        assert distance>=-.5, 'Player escaped behind the active support wall'
        if state['stage']=='enter':
            pawn.add_movement_input(-normal,1,True)
        elif state['stage']=='reverse':
            pawn.add_movement_input(-normal,1,True)
            if elapsed>.35:
                assert distance<1, 'Reverse did not settle at the protected exit plane'
                report['reversal_checks']+=1
                phase('clear')
        elif state['stage']=='clear':
            pawn.add_movement_input(normal,1,True)
            if distance>=90:
                assert 'OUTSIDE' in str(system.get_editor_property('player_crossing_state')), 'Passage state did not clear'
                if report['completed']>=target_count:
                    stop();return
                phase('enter')
        assert elapsed<6, 'Movement timed out in '+state['stage']+'; '+system.get_editor_property('placement_message')
    except Exception:
        stop(traceback.format_exc())

write()
u.portal_reversal_handle=u.register_slate_post_tick_callback(step)
