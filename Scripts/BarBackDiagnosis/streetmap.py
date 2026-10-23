import unreal
bp=unreal.load_asset('/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight')
cdo=unreal.get_default_object(bp.generated_class())
for name in ['L2_EntrySequence','Play Rate']:
 unreal.log('STREETCHECK CDO '+name+'='+str(cdo.get_editor_property(name)))
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
for a in unreal.EditorActorSubsystem().get_all_level_actors():
 if 'BPLM_L2StreetBarFight' in a.get_class().get_name():
  for name in ['L2_EntrySequence','Play Rate','bOverrideStageForTesting','TestStage']:
   try: unreal.log('STREETCHECK MAP '+name+'='+str(a.get_editor_property(name)))
   except Exception as e: unreal.log(str(e))
seq=cdo.get_editor_property('L2_EntrySequence')
if seq:
 t=unreal.AssetExportTask();t.object=seq;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/StreetEntry.t3d';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
