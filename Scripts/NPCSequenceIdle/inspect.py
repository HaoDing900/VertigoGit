import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/NPCSequenceIdle'
paths=['/Game/Characters/ABP_NPCBasic','/Game/Characters/Ulises/ABP_Ulises','/Game/Characters/BowerBird/ABP_Bowerbird_StillNPC','/Game/Characters/IPHA-P/ABP_NPC_IPHAP','/Game/Characters/NPC/Bello/ABP_Bello_NPC','/Game/Characters/NPC/Fell/ABP_Fell_BasicEnmSkeleton']
for p in paths:
 a=unreal.load_asset(p)
 if not a: raise RuntimeError(p)
 for obj,suffix in [(a,''),(unreal.get_default_object(a.generated_class()),'.cdo')]:
  t=unreal.AssetExportTask();t.object=obj;t.filename=str(out/(a.get_name()+suffix+'.t3d'));t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D()
  unreal.log('NPC IDLE EXPORT '+p+suffix+' '+str(unreal.Exporter.run_asset_export_task(t)))
