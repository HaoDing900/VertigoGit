import unreal
for p in ['/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarAfterFight']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/LS_L2BarAfterFight.t3d';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
