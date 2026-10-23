import unreal
for p in ['/Game/Characters/Sa/BP_Cine_Sa_Past','/Game/Characters/BP_StillNPC']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
for a in unreal.EditorActorSubsystem().get_all_level_actors():
 if a.get_name()=='BP_Cine_Sa_Past_C_1':
  unreal.log('PASTCHECK '+a.get_path_name()+' hidden='+str(a.get_editor_property('hidden'))+' loc='+str(a.get_actor_location()))
  for n in ['IsCutscene?(NeedsManualDetect)']:
   try:unreal.log('PASTCHECK '+n+'='+str(a.get_editor_property(n)))
   except Exception as e:unreal.log(str(e))
  for m in a.get_components_by_class(unreal.SkeletalMeshComponent):
   unreal.log('PASTCHECK mesh '+m.get_name()+' visible='+str(m.is_visible())+' hidden='+str(m.get_editor_property('hidden_in_game')))

