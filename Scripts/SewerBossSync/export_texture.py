import unreal
a=unreal.load_asset('/Game/Resource/Texture/c1dbf0d29d05e96fc4b7403f5ca25ef2')
t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/SewerBossSync/caustic-source.png';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
