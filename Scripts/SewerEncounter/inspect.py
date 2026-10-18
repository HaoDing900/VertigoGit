import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerEncounter'
for p in ['/Game/AI/BossAIToolkit/Blueprints/BPC_Boss_Behavior','/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster_L3Sewer','/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg']:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L_SewerUnderApartment')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if 'BPLM_Sewer' in a.get_class().get_name():
  unreal.log('ENCOUNTER REFS '+str(a.get_editor_property('L3_Boss'))+' '+str(a.get_editor_property('L3_BossTrigger')))
