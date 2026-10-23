import unreal
p='/Game/LevelNSequence/In_Level/L2BarStreet/LS_L2BarStreet_IntroScene/LS_L2BarStreet_IntroScene'
a=unreal.load_asset(p)
unreal.log('ENTRYCHECK range='+str(a.get_playback_start_seconds())+'..'+str(a.get_playback_end_seconds()))
t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/StreetEntry.t3d';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
