import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/ActionHUD'
report={}
paths=['/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon1Misto','/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon2Nara','/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky','/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter','/Game/Characters/Monster/SeaMonsterBoss/BP_SeaMonster_Egg']
for p in paths:
 b=unreal.load_asset(p);c=unreal.get_default_object(b.generated_class())
 name=str(c.get_editor_property('EnemyDisplayName'))
 assert name and name!='Enemy', (p,name)
 report[p]={'EnemyDisplayName':name}
reg=unreal.AssetRegistryHelpers.get_asset_registry()
for ad in reg.get_assets_by_class(unreal.TopLevelAssetPath('/Script/EnhancedInput','InputMappingContext')):
 p=str(ad.package_name)
 if not p.startswith('/Game/ThirdPerson/'): continue
 a=ad.get_asset()
 if not a: continue
 entries=[]
 for m in a.get_editor_property('mappings'):
  action=str(m.action)
  if 'IA_Dodge' in action or 'IA_Fire' in action: entries.append({'action':action,'key':str(m.key)})
 if entries:report[p]=entries
(out/'defaults-and-inputs.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf8')
unreal.log('ACTION DEFAULTS PASS: all five enemy names restored from saved assets')
