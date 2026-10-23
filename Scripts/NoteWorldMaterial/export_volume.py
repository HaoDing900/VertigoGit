import unreal
asset=unreal.load_asset('/Game/Blueprints/BP_0ISXInteractiveCollision')
t=unreal.AssetExportTask(); t.object=asset; t.filename=unreal.Paths.project_dir()+'Scripts/NoteWorldMaterial/BP_0ISXInteractiveCollision.copy';t.automated=True;t.prompt=False;t.replace_identical=True;unreal.Exporter.run_asset_export_task(t)
