import unreal
p='/Game/Narrative/Dialogues/Levels/L2BarNStreetOutside/L2_Bar_NPC'
a=unreal.load_asset(p)
t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/L2_Bar_NPC.copy';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
