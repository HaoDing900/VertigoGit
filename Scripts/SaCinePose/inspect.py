import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/SaCinePose'
for p in ['/Game/Characters/Sa/ABP_SaCine','/Game/Characters/Sa/BP_Cine_Sa','/Game/Characters/Sa/BP_StillNPC_Sa']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
 unreal.Exporter.run_asset_export_task(t)
 c=unreal.get_default_object(a.generated_class())
 if isinstance(c,unreal.Character):unreal.log('SACINE CLASS '+p+' '+str(c.mesh.get_editor_property('anim_class')))
