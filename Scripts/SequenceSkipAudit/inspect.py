import unreal,pathlib
p='/Game/ThirdPerson/Blueprints/BP_Player_Sa';a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(pathlib.Path(unreal.Paths.project_dir())/'Scripts/SequenceSkipAudit/BP_Player_Sa.copy');t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
