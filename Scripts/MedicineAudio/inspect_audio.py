import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/MedicineAudio'
paths=['/Game/LevelNSequence/LevelAudio/NE/NE_VTG_Audio_PlayOneShot','/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager','/Game/LevelNSequence/BPLM','/Game/SFX/0InLevel/Apt/SFX_Pill']
for p in paths:
 a=unreal.load_asset(p); assert a
 t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.copy')); t.automated=True; t.prompt=False
 unreal.Exporter.run_asset_export_task(t)
 if isinstance(a,unreal.Blueprint):
  c=unreal.get_default_object(a.generated_class())
  for k in ['VolumeMultiplier','FadeInTime']:
   try: unreal.log('MED_DEFAULT '+a.get_name()+' '+k+' '+str(c.get_editor_property(k)))
   except: pass
ar=unreal.AssetRegistryHelpers.get_asset_registry()
for d in ar.get_assets_by_class(unreal.TopLevelAssetPath('/Script/Engine','World')):
 if 'SaApartment' in str(d.package_name): unreal.log('MED_MAP '+str(d.package_name))
unreal.log('MED_EXPORT_DONE')
