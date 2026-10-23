import unreal, pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketLookDiagnosis'
for p in ['/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket','/Game/Characters/Sa/ABP_Sa','/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/L4Market_PreAdult_Intro_SaObserveIPHA']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 if isinstance(a,unreal.LevelSequence):
  for b in a.get_bindings():
   unreal.log('LOOK BINDING '+b.get_name())
   for tr in b.get_tracks():
    unreal.log('LOOK TRACK '+tr.get_class().get_name())
    for s in tr.get_sections():
     unreal.log('LOOK SECTION '+s.export_text()) if hasattr(s,'export_text') else unreal.log('LOOK SECTION '+str(s))
