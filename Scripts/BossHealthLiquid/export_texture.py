import unreal
a = unreal.load_asset('/Game/EnvArt/Water/Advanced_Water_Material/effects/Caustics/caustics_01_texture')
t = unreal.AssetExportTask()
t.object = a
t.filename = unreal.Paths.project_dir() + 'Scripts/BossHealthLiquid/caustics_01_texture.png'
t.automated = True
t.prompt = False
t.replace_identical = True
unreal.Exporter.run_asset_export_task(t)
