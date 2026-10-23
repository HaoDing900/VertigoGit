import unreal
for name in ['NE_PlayMontage_Start','NE_PlayMontage_InterruptRes']:
 a=unreal.load_asset('/Game/Narrative/Events/Montage/'+name)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BelloSequenceDiagnosis/'+name+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True;unreal.Exporter.run_asset_export_task(t)
