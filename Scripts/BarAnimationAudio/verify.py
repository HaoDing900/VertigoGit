import unreal,re
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/BarAnimationAudio'
items=[('/Game/LevelNSequence/In_Level/L2Bar/ANM/CANM_LS_Sa_KickDrunk1',[.312,1.32]),('/Game/LevelNSequence/In_Level/L2BarStreet/Anim/LSAM_Sa_L2IntroPushDoor',[.31,.74,1.24,1.60]),('/Game/Characters/NPC/GenericOnes/ANM/CosmosMiamiDudes/CosmosDude_Deformed_am_Drunk_Loco_Walk_01_NoRM',[.22,.77,1.37,1.90,2.36,3.04,3.52,3.92])]
for p,times in items:
 a=unreal.load_asset(p);events=unreal.AnimationLibrary.get_animation_notify_events_for_track(a,'Requested_SFX');assert len(events)==len(times)
 assert all(e.get_editor_property('notify').get_editor_property('sound') for e in events)
 dest=out/(a.get_name()+'.verified.copy');t=unreal.AssetExportTask();t.object=a;t.filename=str(dest);t.automated=True;t.prompt=False;assert unreal.Exporter.run_asset_export_task(t)
 raw=dest.read_bytes();txt=raw.decode('utf-16' if raw[:2]==b'\xff\xfe' else 'utf-8-sig')
 for sec in times:assert ('LinkValue=%.6f'%sec) in txt,(p,sec)
 unreal.log('BAR_VERIFIED '+p+' '+str(times))
for p in ['/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision','/Game/LevelNSequence/In_Level/L2BarStreet/BPLM_L2StreetBarFight']:
 a=unreal.load_asset(p);assert a
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;assert unreal.Exporter.run_asset_export_task(t)
 a=unreal.get_default_object(a.generated_class());t.object=a;t.filename=str(out/(a.get_name()+'.defaults.copy'));unreal.Exporter.run_asset_export_task(t)
unreal.log('BAR_VERIFY_SUCCESS')
