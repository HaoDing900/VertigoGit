import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/ClinicMontageDiagnosis'
for name in ['ANM_Sa_Emote_TalkGesture_Montage','ANM_Sa_Emote_TalkGesture_Shrug_Montage','ANM_Sa_Emote_TalkGesture2_Montage']:
 a=unreal.load_asset('/Game/Characters/Sa/Anm/EmoteNGestures/'+name)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(name+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(t)
for path in ['/Game/Narrative/Events/Montage/NE_PlayMontage_InterruptRes','/Game/Narrative/Events/Montage/NE_PlayMontage_Start','/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_Still']:
 c=unreal.load_class(None,path+'.'+path.split('/')[-1]+'_C');a=unreal.get_default_object(c)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(path.split('/')[-1]+'.cdo.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(t)
