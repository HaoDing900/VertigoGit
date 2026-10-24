import unreal,pathlib,re
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/BlackMarketCombat'
paths=['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base','/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_CollisionComponent','/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_AIBehaviorsComponent','/Game/LevelNSequence/In_Level/L4BlackMarket/BPLM_BlackMarket','/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon1Misto','/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon2Nara','/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky']
seen=set()
while paths:
 p=paths.pop(0)
 if p in seen: continue
 seen.add(p); a=unreal.load_asset(p)
 if not a: continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 raw=(out/(a.get_name()+'.copy')).read_bytes(); text=raw.decode('utf16' if raw.startswith(b'\xff\xfe') else 'utf8')
 for line in text.splitlines():
  if line.strip().startswith('ParentClass='):
   pc=line.split(chr(39))[1]
   if pc.startswith('/Game/'): paths.append(pc.split('.')[0])
unreal.log('COMBAT_EXPORT_DONE')

