import unreal
for path in ['/Game/LevelNSequence/BPLM', '/InventorySystemX/Modules/Notes/Components/AC_NotesComponent']:
    asset = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = unreal.Paths.project_dir() + 'Scripts/MailDialogueDiagnosis/' + asset.get_name() + '.copy'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.Exporter.run_asset_export_task(task)
