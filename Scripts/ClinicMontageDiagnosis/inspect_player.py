import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/ClinicMontageDiagnosis'
paths=['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/Characters/Sa/ABP_Sa','/Game/Characters/Sa/Anm/EmoteNGestures/ANM_Sa_Emote_hand_cover_face_singleHand_Montage']
for path in paths:
 a=unreal.load_asset(path)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.log('CLINIC EXPORT '+path+' '+str(unreal.Exporter.run_asset_export_task(t)))

