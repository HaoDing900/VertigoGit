import unreal
p='/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight'
a=unreal.load_asset(p)
t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/street-current.copy';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
for actor in unreal.EditorActorSubsystem().get_all_level_actors():
 if 'BPLM_L2StreetBarFight' in actor.get_class().get_name():
  c=actor.get_component_by_class(unreal.VTGLevelEntrySequenceComponent)
  unreal.log('ENTRYCONFIG component='+str(c))
  if c:
   for n in ['entry_sequence','start_policy','play_on_begin_play','skip_on_save_resume']:
    unreal.log('ENTRYCONFIG '+n+'='+str(c.get_editor_property(n)))
