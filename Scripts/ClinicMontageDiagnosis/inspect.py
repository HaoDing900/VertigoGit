import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/ClinicMontageDiagnosis'
paths=['/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_Still','/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic','/Game/Narrative/Events/Montage/NE_PlayMontage_Start','/Game/Narrative/Events/Montage/NE_PlayMontage_EndOrTran','/Game/Narrative/Events/Montage/NE_PlayMontage_InterruptRes']
for path in paths:
 a=unreal.load_asset(path)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.log('CLINIC EXPORT '+path+' '+str(unreal.Exporter.run_asset_export_task(t)))
