import unreal
for n in ['CANM_Sa_SittingOnBarChair_Talk_casc_Montage','CANM_Sa_SittingOnBarChair_Negative_casc_Montage','CANM_Sa_SittingOnBarChair_SlightHeadache1_Montage']:
 a=unreal.load_asset('/Game/LevelNSequence/In_Level/L2Bar/ANM/Sa/'+n)
 unreal.log('MONTAGECHECK '+n+' auto='+str(a.get_editor_property('enable_auto_blend_out')))
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/'+n+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
