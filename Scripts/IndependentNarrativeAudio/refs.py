import unreal
ar=unreal.AssetRegistryHelpers.get_asset_registry()
for name in ['NE_VTG_Audio_PlayOneShot','NE_VTG_Audio_StopOneShot']:
 p='/Game/LevelNSequence/LevelAudio/NE/'+name
 unreal.log('INDEPENDENT_REFS '+name+' '+str(ar.get_referencers(p,unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True))))
