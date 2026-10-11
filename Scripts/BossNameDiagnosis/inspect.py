import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BossNameDiagnosis'
for p in ['/Game/Characters/Monster/SeaMonsterBoss/BP_Boss_SeaMonster','/Game/Widget/BossHealthBar/WBP_BossHealthBar']:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
