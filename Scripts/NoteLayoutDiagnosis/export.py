import unreal
for name in ['WB_Note','WB_TextContainer','WB_NoteFullScreen']:
    asset=unreal.load_asset('/Game/Inventory/InventoryXBased/UMG/'+name)
    task=unreal.AssetExportTask()
    task.object=asset
    task.filename=unreal.Paths.project_dir()+'Scripts/NoteLayoutDiagnosis/'+name+'.copy'
    task.automated=True
    task.prompt=False
    task.replace_identical=True
    unreal.Exporter.run_asset_export_task(task)
