import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
a=next(x for x in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if x.get_name()=='BP_Bello_NPC_C_1')
m=a.get_component_by_class(unreal.SkeletalMeshComponent)
s=unreal.load_asset('/Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_BelloTouchCarBottom')
p,sa=unreal.LevelSequencePlayer.create_level_sequence_player(w,s,unreal.MovieSceneSequencePlaybackSettings())
def report(label):
 ai=m.get_anim_instance()
 unreal.log('PROBE '+label+' actor='+a.get_path_name()+' mesh='+m.get_path_name()+' mode='+str(m.get_editor_property('animation_mode'))+' anim='+str(ai))
 if ai:
  try: unreal.log('PROBE bool='+str(ai.get_editor_property('Is Cutscene')))
  except Exception as e: unreal.log('PROBE bool error '+str(e))
 for bone in ['pelvis','spine_02','c_root_master.x','c_spine_01.x']:
  unreal.log('PROBE BONE '+bone+' '+str(m.get_socket_transform(bone,unreal.RelativeTransformSpace.RTS_COMPONENT)))
report('before')
for f in [0,1,5,30,60,100,200,300]:
 p.set_playback_position(unreal.MovieSceneSequencePlaybackParams(frame=unreal.FrameTime(unreal.FrameNumber(f)),update_method=unreal.UpdatePositionMethod.JUMP))
 report('frame '+str(f))
p.stop()
