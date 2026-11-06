import unreal
import json
import os

report = {}
def sample(label, fn):
    try:
        value=fn()
        report[label]=str(value)
        unreal.log('SA_FOOT_LIVE '+label+'='+str(value))
        return value
    except Exception as exc:
        report[label]={'unavailable':str(exc)}
        unreal.log('SA_FOOT_LIVE '+label+' unavailable: '+str(exc))
        return None

def prop(obj,name):
    return obj.get_editor_property(name)

world=sample('game_world', lambda:unreal.EditorLevelLibrary.get_game_world())
if world:
    sa=sample('player',lambda:unreal.GameplayStatics.get_player_character(world,0))
    pc=sample('controller',lambda:unreal.GameplayStatics.get_player_controller(world,0))
    if sa:
        sample('actor_location',lambda:sa.get_actor_location())
        sample('velocity',lambda:sa.get_velocity())
        sample('actor_collision',lambda:sa.get_actor_enable_collision())
        sample('actor_hidden',lambda:prop(sa,'hidden'))
        for flag in ['HidePlayerNCam','IsAttacking','IsDodging?','IsDead?','InCombat','IsInCineCutscene_LockInput(NotWorkForMont)']:
            sample('flag.'+flag,lambda flag=flag:prop(sa,flag))
        move=sample('movement',lambda:sa.get_character_movement())
        if move:
            sample('movement.mode',lambda:prop(move,'movement_mode'))
            sample('movement.on_ground',lambda:move.is_moving_on_ground())
            sample('movement.floor',lambda:prop(move,'current_floor'))
        capsule=sample('capsule',lambda:sa.get_capsule_component())
        if capsule:
            sample('capsule.collision',lambda:capsule.get_collision_enabled())
            sample('capsule.profile',lambda:capsule.get_collision_profile_name())
        mesh=sample('mesh',lambda:prop(sa,'mesh'))
        if mesh:
            sample('mesh.location',lambda:mesh.get_world_location())
            sample('mesh.relative_location',lambda:prop(mesh,'relative_location'))
            sample('mesh.animation_mode',lambda:prop(mesh,'animation_mode'))
            sample('mesh.pause_anims',lambda:prop(mesh,'pause_anims'))
            sample('mesh.tick_enabled',lambda:mesh.is_component_tick_enabled())
            sample('mesh.visible',lambda:mesh.is_visible())
            sample('mesh.visibility_tick_option',lambda:prop(mesh,'visibility_based_anim_tick_option'))
            anim=sample('anim.instance',lambda:mesh.get_anim_instance())
            if anim:
                sample('anim.class',lambda:anim.get_class().get_path_name())
                for name in ['IsCutscene','Should Move','Ground Speed','Velocity','IsFalling']:
                    sample('anim.'+name,lambda name=name:prop(anim,name))
                sample('anim.active_montage',lambda:anim.get_current_active_montage())
            for bone in ['foot_l','foot_r']:
                sample('bone.'+bone+'.exists',lambda bone=bone:mesh.does_socket_exist(bone))
                foot=sample('bone.'+bone+'.location',lambda bone=bone:mesh.get_socket_location(bone))
                if foot:
                    start=unreal.Vector(foot.x,foot.y,foot.z+35)
                    end=unreal.Vector(foot.x,foot.y,foot.z-120)
                    for complex_trace in [True,False]:
                        sample('trace.'+bone+'.complex='+str(complex_trace),lambda complex_trace=complex_trace:unreal.SystemLibrary.line_trace_single(world,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,complex_trace,[sa],unreal.DrawDebugTrace.NONE,True))
        components=sample('components',lambda:sa.get_components_by_class(unreal.ActorComponent))
        if components:
            for comp in components:
                cname=comp.get_class().get_name()
                if 'VTGPlayerAudioComponent' in cname:
                    for name in ['FootstepVolume','DefaultSteps','ConcreteSteps','BarefootSteps','BarefootMesh','StepAttenuation']:
                        sample('audio.'+name,lambda comp=comp,name=name:prop(comp,name))
                if 'NarrativeComponent' in cname:
                    sample('narrative.current_dialogue',lambda:comp.get_current_dialogue())
                    sample('narrative.sequence_player',lambda:comp.get_dialogue_level_sequence_player())
    if pc:
        sample('camera.view_target',lambda:pc.get_view_target())
        sample('camera.view_point',lambda:pc.get_player_view_point())
    actors=sample('sequence.actors',lambda:unreal.GameplayStatics.get_all_actors_of_class(world,lambda:unreal.LevelSequenceActor()))
    if actors:
        for i,actor in enumerate(actors):
            player=sample('sequence.'+str(i)+'.player',lambda:actor.get_sequence_player())
            if player:
                sample('sequence.'+str(i)+'.playing',lambda:player.is_playing())
                sample('sequence.'+str(i)+'.paused',lambda:player.is_paused())
output=unreal.Paths.project_dir()+'Scripts/PoliceNotifyObservation/live_snapshot.json'
os.makedirs(os.path.dirname(output),exist_ok=True)
with open(output,'w',encoding='utf-8') as stream:json.dump(report,stream,ensure_ascii=False,indent=2)
unreal.log('SA_FOOT_LIVE snapshot saved: '+output)
