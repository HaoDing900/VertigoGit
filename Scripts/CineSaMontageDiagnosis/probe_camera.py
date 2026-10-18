import unreal, time, traceback
unreal.log('CAMPROBE START')
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L_SewerUnderApartment')
# Control run: set the intended stage before BeginPlay, in memory only.
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_class().get_name()=='BPLM_SewerUnderApt_C':
        a.set_editor_property('bOverrideStageForTesting',True)
        a.set_editor_property('TestStage','L3Sewer')
        unreal.log('CAMPROBE CONTROL stage resolved before BeginPlay; no asset save')
started=time.monotonic()
phase=0
last=None
def probe_tick(dt):
    global phase,last,handle
    try:
        elapsed=time.monotonic()-started
        if elapsed>25:
            unreal.log('CAMPROBE DONE')
            unreal.unregister_slate_post_tick_callback(handle)
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            unreal.SystemLibrary.quit_editor()
            return
        if phase==0:
            phase=1
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_play_simulate()
            return
        world=unreal.EditorLevelLibrary.get_game_world()
        if not world:return
        actors=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor)
        manager=next((a for a in actors if a.get_class().get_name()=='BPLM_SewerUnderApt_C'),None)
        pc=unreal.GameplayStatics.get_player_controller(world,0)
        player=next((a for a in actors if a.get_class().get_name()=='BP_Player_Sa_C'),None)
        trigger=next((a for a in actors if a.get_class().get_name()=='BPA_0SmartTrigger_C'),None)
        cam=next((a for a in actors if a.get_name().startswith('BP_1CameraLockAutoTrigger_C_3')),None)
        if not(manager and pc and player and trigger):return
        point=trigger.get_editor_property('Lock Cam Point')
        state=(str(pc.get_view_target()),str(manager.get_editor_property('bCameraLocked')),str(manager.get_editor_property('ActiveCameraPoint')),str(point))
        if state!=last:
            last=state;unreal.log('CAMPROBE STATE %.3f %s'%(elapsed,str(state)))
        if phase==1 and elapsed>5:
            phase=2
            unreal.log('CAMPROBE VALID camera='+str(unreal.SystemLibrary.is_valid(point))+' cameraInActors='+str(point in actors)+' PC='+str(manager.get_editor_property('Player Controller'))+' pawn='+str(manager.get_editor_property('Player Pawn')))
            pc.possess(player)
            manager.call_method('ReceiveBeginPlay')
            unreal.log('CAMPROBE TELEPORT trigger='+str(trigger.get_actor_location())+' player='+str(player.get_actor_location())+' stage='+str(manager.get_active_stage()))
            target=trigger.get_actor_location()+unreal.Vector(0,0,100)
            player.set_actor_location(target,False,True)
        if phase==2 and elapsed>15:
            phase=3
            unreal.log('CAMPROBE DIRECT LOCK '+str(point))
            if point:
                manager.call_method('LockCameraPoint',(point,))
                unreal.log('CAMPROBE LOCK IMMEDIATE '+str(manager.get_editor_property('bCameraLocked'))+' valid='+str(unreal.SystemLibrary.is_valid(point)))
    except Exception:
        unreal.log_error('CAMPROBE '+traceback.format_exc())
        unreal.unregister_slate_post_tick_callback(handle)
        unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(probe_tick)
