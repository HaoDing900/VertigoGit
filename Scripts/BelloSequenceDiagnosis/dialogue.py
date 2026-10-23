import unreal
p='/Game/Narrative/Dialogues/Levels/L3PoliceStation/L3PoliceStation_Still'
a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BelloSequenceDiagnosis/L3PoliceStation_Still.copy';t.automated=True;t.prompt=False;t.replace_identical=True;unreal.Exporter.run_asset_export_task(t)
