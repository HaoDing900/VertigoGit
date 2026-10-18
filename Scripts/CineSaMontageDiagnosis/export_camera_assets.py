import unreal
for path in ['/Game/Narrative/Dialogues/Levels/L3Sewer/L3Sewer_Still','/Game/LevelNSequence/BPLM']:
    a=unreal.load_asset(path)
    t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/CineSaMontageDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
    unreal.Exporter.run_asset_export_task(t)
