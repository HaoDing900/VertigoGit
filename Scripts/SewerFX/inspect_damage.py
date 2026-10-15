import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerFX'
for p in ['/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Default','/Game/AI/BossAIToolkit/Blueprints/DamageTypes/BP_DamageType_Poison','/Game/AI/BossAIToolkit/Blueprints/Interfaces/BI_DamageType','/Game/AI/BossAIToolkit/Blueprints/Interfaces/BI_Boss']:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
