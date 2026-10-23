import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
for actor in unreal.EditorActorSubsystem().get_all_level_actors():
 if 'BPLM_L2StreetBarFight' in actor.get_class().get_name():
  for c in actor.get_components_by_class(unreal.VTGLevelEntrySequenceComponent):
   unreal.log('ANMCHECK '+c.get_name()+' sequence='+str(c.get_editor_property('entry_sequence')))
for p in ['/Game/Characters/Sa/ABP_Sa','/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_PreDrunkFight']:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
