import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/HintSkipIntegration'
for name in ['WB_ControlsHints','WB_ControlsHintBlock','WB_ControlsHintKeyImage']:
 a=unreal.load_asset('/Game/Inventory/InventoryXBased/UMG/Inventory/ControlsHints/'+name)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(name+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
