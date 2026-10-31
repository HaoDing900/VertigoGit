import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/MayYardPose'
paths=['/Game/LevelNSequence/In_Level/L5Clinic/ShoppingCenterYard/BPLM_ShoppingCenter_ClinicYard','/Game/Narrative/Dialogues/Levels/L5Clinic/L5ClinicYard_Still','/Game/Characters/May/ABP_NPC_May','/Game/Characters/May/BP_Cine_May','/Game/Characters/May/BP_May_NPC']
for p in paths:
 a=unreal.load_asset(p)
 if not a: raise RuntimeError(p)
 for obj,suffix in [(a,''),(unreal.get_default_object(a.generated_class()),'.cdo')]:
  t=unreal.AssetExportTask();t.object=obj;t.filename=str(out/(a.get_name()+suffix+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
  unreal.log('MAY EXPORT '+p+suffix+' '+str(unreal.Exporter.run_asset_export_task(t)))
