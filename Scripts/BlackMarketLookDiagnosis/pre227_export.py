import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketLookDiagnosis'
for p in ['/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/NLS_L4Market_PreAdult_IPHAPauseInfrontOfAdult2','/Game/Narrative/Events/1PlayLevelSequence']:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.visibility.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
