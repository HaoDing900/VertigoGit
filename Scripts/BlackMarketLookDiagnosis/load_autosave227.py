import unreal
p=unreal.Paths.project_saved_dir()+'Autosaves/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation_Auto3.uasset'
pkg=unreal.load_package(p);unreal.log('AUTOPACK '+str(pkg))
if pkg:
 for a in [unreal.find_object(pkg,'L4_BlackMarketInvestigation_Still_MainInvestigation')]:
  if isinstance(a,unreal.Blueprint):
   unreal.log('AUTOPACK blueprint '+a.get_path_name());t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BlackMarketLookDiagnosis/autosave227.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)

