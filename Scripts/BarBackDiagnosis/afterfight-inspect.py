import unreal
p='/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight'
a=unreal.load_asset(p)
t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/street-afterfight-current.copy';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
