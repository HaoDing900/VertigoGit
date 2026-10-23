import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
for actor in unreal.EditorActorSubsystem().get_all_level_actors():
 if 'BPLM_L2StreetBarFight' in actor.get_class().get_name():
  for c in actor.get_components_by_class(unreal.VTGLevelEntrySequenceComponent):
   unreal.log('ENTRYALL '+c.get_name())
   for n in ['entry_sequence','start_policy','play_on_begin_play','skip_on_save_resume']:
    unreal.log('ENTRYALL '+n+'='+str(c.get_editor_property(n)))
