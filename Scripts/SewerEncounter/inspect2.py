import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerEncounter'
a=unreal.load_asset('/Game/AI/BossAIToolkit/Blueprints/Enums/E_CombatTrigger');t=unreal.AssetExportTask();t.object=a;t.filename=str(out/'CombatTrigger.t3d');t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L_SewerUnderApartment')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if 'SmartTrigger' in a.get_class().get_name():
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 if 'Boss_SeaMonster_L3Sewer' in a.get_class().get_name():
  for c in a.get_components_by_class(unreal.ActorComponent):
   if 'BPC_Boss_Behavior' in c.get_class().get_name():unreal.log('BOSS ACTUAL CombatTrigger='+str(c.get_editor_property('CombatTrigger')))
