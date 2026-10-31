import unreal
from pathlib import Path
out=[]
targets=['UE5RTG_EnmAI_Mannequin','Ulises_RIG_Skeleton','Bowerbird_RIG_ue_renamed__Skeleton','IPHA-Philip_Rig__Skeleton','Bello_Rig_UERenamed__Skeleton']
reg=unreal.AssetRegistryHelpers.get_asset_registry()
for a in reg.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine','AnimSequence'),True):
 name=str(a.asset_name)
 if not any(x in name.lower() for x in ['idle','default','standby']):continue
 sk=a.get_tag_value('Skeleton') or ''
 if any(t in sk for t in targets):out.append(name+' | '+str(a.package_name)+' | '+sk)
for p in ['/Game/Characters/BowerBird/BP_Bowerbird_Still','/Game/Characters/Ulises/BP_Ulises_NPC','/Game/Characters/IPHA-P/BP_Philip_IPHA_NPC','/Game/Characters/NPC/Bello/BP_Bello_NPC','/Game/Characters/NPC/Fell/BP_Fell']:
 a=unreal.load_asset(p)
 if a:
  c=unreal.get_default_object(a.generated_class())
  try:out.append('ACTOR '+p+' idle='+str(c.get_editor_property('IdleAnim'))+' mesh='+str(c.mesh.get_editor_property('skeletal_mesh_asset')))
  except Exception as e:out.append(str(e))
Path(unreal.Paths.project_dir(),'Scripts/NPCSequenceIdle/idles.txt').write_text('\n'.join(out),encoding='utf-8')
unreal.log('NPC IDLE CANDIDATES '+str(len(out)))
