import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
a=next(x for x in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if x.get_name()=='BP_Bello_NPC_C_1')
m=a.get_component_by_class(unreal.SkeletalMeshComponent)
m.set_update_animation_in_editor(True)
s=unreal.load_asset('/Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_BelloTouchCarBottom')
p,sa=unreal.LevelSequencePlayer.create_level_sequence_player(w,s,unreal.MovieSceneSequencePlaybackSettings())
count=0
frames=[30,100,200,300]
def tick(dt):
 global count
 try:
  if count%30==0:
   f=frames[min(count//30,3)]
   p.set_playback_position(unreal.MovieSceneSequencePlaybackParams(frame=unreal.FrameTime(unreal.FrameNumber(f)),update_method=unreal.UpdatePositionMethod.JUMP))
  if count%30==29:
   ai=m.get_anim_instance()
   unreal.log('LIVEPROBE count='+str(count)+' bool='+str(ai.get_editor_property('Is Cutscene'))+' montage='+str(ai.get_current_active_montage())+' spine='+str(m.get_socket_transform('spine_02',unreal.RelativeTransformSpace.RTS_COMPONENT)))
   for prop in ['AnimGraphNode_BlendListByBool','AnimGraphNode_Slot','AnimGraphNode_Slot_0','AnimGraphNode_Slot_1','AnimGraphNode_Slot_6']:
    try: unreal.log('LIVEPROBE '+prop+' '+str(ai.get_editor_property(prop)))
    except: pass
  count+=1
  if count>=120:
   unreal.unregister_slate_post_tick_callback(handle); unreal.SystemLibrary.quit_editor()
 except Exception as e:
  unreal.log_error('LIVEPROBE '+str(e));unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
handle=unreal.register_slate_post_tick_callback(tick)
