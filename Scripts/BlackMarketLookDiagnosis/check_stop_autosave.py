import unreal
paths=['/Temp/Autosaves/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation_Auto3','/Game/Narrative/Events/NE_StopLevelSequence']
for p in paths:
 a=unreal.load_asset(p)
 unreal.log('AUTOFLOW asset='+str(a))
 if a:
  t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BlackMarketLookDiagnosis/'+a.get_name()+'.stopcheck.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
