import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/MainMenuButtonArt'
p='/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu';a=unreal.load_asset(p)
t=unreal.AssetExportTask();t.object=a;t.filename=str(out/'WBP_MainMenu.copy');t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
