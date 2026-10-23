import unreal,time,traceback
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
cdo=unreal.get_default_object(unreal.load_class(None,'/Game/Narrative/Events/1PlayLevelSequence.1PlayLevelSequence_C'))
suspend=cdo.get_editor_property('Suspend Montages During Sequence')
unreal.log('PIEPROBE event default suspend='+str(suspend))
s=unreal.load_asset('/Game/LevelNSequence/In_Level/L3PoliceStation/NLS_L3_BelloTouchCarBottom')
phase=0;started=time.monotonic();last=-1

def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
 unreal.SystemLibrary.quit_editor()

def tick(dt):
 global phase,started,last,a,m,p,comp,idle
 try:
  if time.monotonic()-started>45:finish();return
  if phase==0:
   phase=1;unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_play_simulate();return
  w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
  if not w:return
  if phase==1:
   actors=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor)
   a=next((x for x in actors if x.get_name()=='BP_Bello_NPC_C_1'),None)
   pc=unreal.GameplayStatics.get_player_controller(w,0)
   if not a or not pc:return
   m=a.get_component_by_class(unreal.SkeletalMeshComponent)
   m.set_editor_property('visibility_based_anim_tick_option',unreal.VisibilityBasedAnimTickOption.ALWAYS_TICK_POSE_AND_REFRESH_BONES)
   comp=pc.get_component_by_class(unreal.NarrativeComponent)
   if not comp:
    comp=unreal.new_object(unreal.NarrativeComponent,outer=pc)
   idle=unreal.load_asset('/Game/LevelNSequence/In_Level/L3PoliceStation/ANM/Bello/ANM_Bello_Stand_Idle_still1_Montage')
   unreal.log('PIEPROBE prior idle play='+str(m.get_anim_instance().montage_play(idle)))
   m.get_anim_instance().montage_set_next_section('Default','Default',idle)
   p=comp.play_dialogue_level_sequence(s,False,False,1.2,0,True,False,30.0,suspend)
   unreal.log('PIEPROBE start actor='+a.get_path_name()+' player='+str(p)+' component='+str(comp))
   phase=2;started=time.monotonic();return
  sec=int(time.monotonic()-started)
  if sec!=last:
   last=sec;ai=m.get_anim_instance();mont=ai.get_current_active_montage()
   unreal.log('PIEPROBE idle active='+str(ai.montage_is_active(idle)))
   if sec==8:
    ai.montage_stop(0.0,idle);unreal.log('PIEPROBE isolated control: stop only prior idle')
   unreal.log('PIEPROBE t='+str(sec)+' bool='+str(ai.get_editor_property('Is Cutscene'))+' montage='+str(mont)+' mode='+str(m.get_editor_property('animation_mode'))+' spine='+str(m.get_socket_transform('spine_02',unreal.RelativeTransformSpace.RTS_COMPONENT)))
   unreal.log('PIEPROBE guards='+str([x.get_path_name() for x in a.get_components_by_class(unreal.ActorComponent) if 'MontageGuard' in x.get_class().get_name()]))
  if sec>=16:finish()
 except:
  unreal.log_error('PIEPROBE '+traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)

