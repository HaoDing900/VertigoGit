import unreal
paths=['/Game/Narrative/Events/Montage/NE_PlayMontage_InterruptRes','/Game/Narrative/Events/Montage/NE_PlayMontage_Start','/Game/Characters/Sa/ABP_SaCine','/Game/LevelNSequence/In_Level/L2Bar/ANM/Sa/CANM_Sa_SittingOnBarChair_Negative_casc_Montage']
for p in paths:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/BarBackDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
 unreal.Exporter.run_asset_export_task(t)
