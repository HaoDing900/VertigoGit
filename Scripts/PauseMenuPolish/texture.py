import unreal
x=unreal.load_asset('/Game/2DArt/UI/GameOption/Pause_copy')
t=unreal.AssetExportTask();t.object=x;t.filename=unreal.Paths.project_dir()+'Scripts/PauseMenuPolish/Pause_copy.png';t.automated=True;t.prompt=False
unreal.Exporter.run_asset_export_task(t)
