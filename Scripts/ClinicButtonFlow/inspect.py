import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/ClinicButtonFlow'
for p in ['/Game/LevelNSequence/In_Level/L5Clinic/BPLM_Clinic','/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_Still','/Game/Blueprints/BP_0ISXInteractiveCollision']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.Exporter.run_asset_export_task(t)
