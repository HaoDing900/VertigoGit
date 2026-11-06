import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/FistWind'
for p in ['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/VFX/Wind_VFX/VFX/Particles/NS_Wind_Flows_02_Fast']:
 a=unreal.load_asset(p); assert a,p
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.copy')); t.automated=True; t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
unreal.log('FIST_INSPECT_DONE')
