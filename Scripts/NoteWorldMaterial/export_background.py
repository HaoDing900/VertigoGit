import unreal
for path in ['/InventorySystemX/Modules/Notes/DataAssets/PDA_NoteText','/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt','/InventorySystemX/Modules/Notes/Components/AC_NotesComponent','/InventorySystemX/Modules/Notes/Actors/BP_NoteMeshBackground']:
    asset=unreal.load_asset(path)
    task=unreal.AssetExportTask()
    task.object=asset
    task.filename=unreal.Paths.project_dir()+'Scripts/NoteWorldMaterial/'+asset.get_name()+'.copy'
    task.automated=True
    task.prompt=False
    task.replace_identical=True
    unreal.Exporter.run_asset_export_task(task)

