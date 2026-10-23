import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketLookDiagnosis'
for p in ['/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket','/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation']:
 a=unreal.load_asset(p);t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.current.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
