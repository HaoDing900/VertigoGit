import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SaLookDiagnosis'
for p in ['/Game/Characters/Sa/ABP_Sa','/Game/ThirdPerson/Blueprints/BP_Player_Sa']:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
