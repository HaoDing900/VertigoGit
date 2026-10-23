import unreal
asset=unreal.load_asset('/Narrative/DefaultConditions/NC_HasDialogueNodePlayed')
t=unreal.AssetExportTask();t.object=asset;t.filename=unreal.Paths.project_dir()+'Scripts/MailDialogueDiagnosis/NC_HasDialogueNodePlayed.copy';t.automated=True;t.prompt=False;t.replace_identical=True
unreal.Exporter.run_asset_export_task(t)
