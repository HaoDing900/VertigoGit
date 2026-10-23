import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketLookDiagnosis'
reg=unreal.AssetRegistryHelpers.get_asset_registry();reg.search_all_assets(True)
paths=['/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket','/Game/Narrative/Dialogues/Levels/L4BlackMarket/L4_BlackMarketInvestigation_Still_MainInvestigation','/Game/ThirdPerson/Blueprints/BP_Player_Sa']
for d in reg.get_all_assets():
 if any(x in str(d.asset_name).lower() for x in ['playerlight','characterunlit']):
  unreal.log('DIAG ASSET '+str(d.package_name));paths.append(str(d.package_name))
for p in paths:
 a=unreal.load_asset(p)
 if not a:continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.latest.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 if 'PlayerLight' in a.get_name():
  for r in reg.get_referencers(a.get_outermost().get_name(),unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)):
   unreal.log('DIAG REF '+str(r))
