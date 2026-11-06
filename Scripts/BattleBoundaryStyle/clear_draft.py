import unreal
p='/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram'
if unreal.EditorAssetLibrary.does_asset_exist(p): assert unreal.EditorAssetLibrary.delete_asset(p)
