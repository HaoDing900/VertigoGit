import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/HintSkipIntegration'
a=unreal.load_asset('/Game/Blueprints/Widget/WBP_HintLayer')
t=unreal.AssetExportTask();t.object=a;t.filename=str(out/'WBP_HintLayer.copy');t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
