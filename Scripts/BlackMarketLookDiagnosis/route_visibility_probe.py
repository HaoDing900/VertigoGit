import unreal,math,pathlib
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
sa=next(a for a in actors if a.get_class().get_name()=='BP_Player_Sa_C');mesh=sa.get_editor_property('Mesh')
m=next(a for a in actors if a.get_class().get_name()=='BPLM_BlackMarket_C');m.set_editor_property('Player Pawn',sa)
def status(label):
 unreal.log('ROUTEVIS '+label+' actorhidden='+str(sa.get_editor_property('hidden'))+' meshvisible='+str(mesh.is_visible())+' meshhidden='+str(mesh.get_editor_property('hidden_in_game'))+' ownernosee='+str(mesh.get_editor_property('owner_no_see'))+' location='+str(sa.get_actor_location()))
status('initial')
for name in ['NLS_L4Market_PreAdult_IPHAPauseInfrontOfAdult1','NLS_L4Market_PreAdult_IPHAPauseInfrontOfAdult2']:
 seq=unreal.load_asset('/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/'+name)
 task=unreal.AssetExportTask();task.object=seq;task.filename=unreal.Paths.project_dir()+'Scripts/BlackMarketLookDiagnosis/'+name+'.route.copy';task.automated=True;task.prompt=False;unreal.Exporter.run_asset_export_task(task)
 settings=unreal.MovieSceneSequencePlaybackSettings();settings.restore_state=True
 p,a=unreal.LevelSequencePlayer.create_level_sequence_player(world,seq,settings)
 p.play();p.pause();status(name+' start')
 f=max(0,seq.get_playback_end()-1)
 params=unreal.MovieSceneSequencePlaybackParams(frame=unreal.FrameTime(unreal.FrameNumber(f)),position_type=unreal.MovieScenePositionType.FRAME,update_method=unreal.UpdatePositionMethod.JUMP)
 p.set_playback_position(params);status(name+' end');p.stop();status(name+' stopped')
comp=sa.get_component_by_class(unreal.NarrativeComponent)
dlg=unreal.new_object(unreal.load_class(None,'/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation.L4_BlackMarketInvestigation_Still_MainInvestigation_C'),outer=m);dlg.set_editor_property('OwningComp',comp)
dlg.call_method('OnDialogueNode Started/Finished Playing - L4_BlackMarketInvestigation_Still_MainInvestigation_DialogueNode_NPC_227',(None,True));status('node227')
cam=next(a for a in actors if a.get_name()=='BP_CameraLockPoint_C_3').get_component_by_class(unreal.CameraComponent)
local=unreal.MathLibrary.inverse_transform_location(cam.get_world_transform(),sa.get_actor_location())
fov=cam.get_editor_property('field_of_view');sx=0.5+local.y/(2*local.x*math.tan(math.radians(fov/2)))
unreal.log('ROUTEVIS camera_local='+str(local)+' fov='+str(fov)+' normalized_screen_x='+str(sx))
