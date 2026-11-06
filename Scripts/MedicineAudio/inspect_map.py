import unreal
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/MedicineAudio'
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/SaApartment')
for a in unreal.EditorLevelLibrary.get_all_level_actors():
 if any(s in a.get_class().get_name() for s in ['BPLM','AudioManager']):
  unreal.log('MED_ACTOR '+a.get_path_name()+' '+a.get_class().get_path_name())
  t=unreal.AssetExportTask(); t.object=a; t.filename=str(out/(a.get_name()+'.actor.copy')); t.automated=True; t.prompt=False
  unreal.Exporter.run_asset_export_task(t)
  for c in a.get_components_by_class(unreal.AudioComponent):
   unreal.log('MED_COMPONENT '+c.get_path_name()+' volume='+str(c.get_editor_property('volume_multiplier'))+' sound='+str(c.get_editor_property('sound')))
unreal.log('MED_MAP_DONE')
