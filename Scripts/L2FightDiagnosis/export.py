import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/L2FightDiagnosis'
for p in ['/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
