import unreal
for p in ['/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight','/Game/Widget/Loading/WBP_VTGLoadingScreen']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
