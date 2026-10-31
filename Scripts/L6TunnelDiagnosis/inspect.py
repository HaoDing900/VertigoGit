import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/L6TunnelDiagnosis'
paths=['/Game/Narrative/Events/1PlayLevelSequence','/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/Blueprints/BP_0CameraLockPoint']
for p in paths:
 a=unreal.load_asset(p)
 if not a: continue
 for obj,suffix in [(a,''),(unreal.get_default_object(a.generated_class()),'.cdo')]:
  t=unreal.AssetExportTask();t.object=obj;t.filename=str(out/(a.get_name()+suffix+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
  unreal.log('L6 EXPORT '+p+suffix+' '+str(unreal.Exporter.run_asset_export_task(t)))
