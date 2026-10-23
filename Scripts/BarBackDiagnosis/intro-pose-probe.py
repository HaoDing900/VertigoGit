import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
a=next(x for x in unreal.EditorActorSubsystem().get_all_level_actors() if x.get_class().get_name()=='BP_Player_Sa_C')
m=a.get_component_by_class(unreal.SkeletalMeshComponent)
def report(label):
 ai=m.get_anim_instance()
 unreal.log('POSECHECK '+label+' mesh='+m.get_name()+' anim='+str(ai)+' mode='+str(m.get_editor_property('animation_mode')))
 if ai:
  try: unreal.log('POSECHECK cutscene='+str(ai.get_editor_property('IsCutscene'))+' montage='+str(ai.get_current_active_montage()))
  except Exception as e: unreal.log('POSECHECK '+str(e))
report('before')
s=unreal.load_asset('/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_IntroScene/FLS_L2BarStreet_IntroScene_SkeletalAnim')
p,act=unreal.LevelSequencePlayer.create_level_sequence_player(w,s,unreal.MovieSceneSequencePlaybackSettings())
p.play();p.pause()
report('firstframe')
params=unreal.MovieSceneSequencePlaybackParams()
params.frame=unreal.FrameTime(unreal.FrameNumber(30))
params.position_type=unreal.MovieScenePositionType.FRAME
params.update_method=unreal.UpdatePositionMethod.JUMP
p.set_playback_position(params)
report('frame30')
p.stop()
