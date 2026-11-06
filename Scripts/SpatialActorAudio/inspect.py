import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/SpatialActorAudio'
for p in ['/Game/LevelNSequence/In_Level/L3PoliceStation/BPLM_PoliceStation','/Game/Characters/Drone_IPHA/BP_Drone_IPHA_NPC','/Game/Characters/NPC/Bello/BP_Bello_NPC','/Game/SFX/IPHA/Electric_Hum_Loop_E']:
 a=unreal.load_asset(p); assert a,p
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.copy')); t.automated=True; t.prompt=False
 assert unreal.Exporter.run_asset_export_task(t)
unreal.log('SPATIAL_INSPECT_DONE')
