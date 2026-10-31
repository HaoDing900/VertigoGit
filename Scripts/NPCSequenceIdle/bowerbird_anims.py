import unreal
from pathlib import Path
out=[]
for a in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine','AnimSequence'),True):
 sk=a.get_tag_value('Skeleton') or ''
 if 'Bowerbird_RIG_ue_renamed__Skeleton' in sk:out.append(str(a.package_name))
Path(unreal.Paths.project_dir(),'Scripts/NPCSequenceIdle/bowerbird-anims.txt').write_text('\n'.join(out),encoding='utf-8')
