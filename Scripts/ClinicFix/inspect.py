import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/ClinicFix'
for p in ['/Game/Characters/BP_StillNPC','/Game/Characters/Sa/BP_Cine_Sa','/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/NLS_L5ClinicYard_SaWalkTowardsMay']:
 a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.Exporter.run_asset_export_task(t)
