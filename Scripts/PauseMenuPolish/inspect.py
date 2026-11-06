import unreal
for name,path in [('Main','/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu'),('Pause','/Game/ThirdPerson/Blueprints/UI/WBP_PauseMenu')]:
 a=unreal.load_asset(path)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/PauseMenuPolish/'+name+'.copy';t.automated=True;t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
