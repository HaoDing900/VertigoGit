import unreal,json,hashlib
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/BattleBoundaryStyle'
for mp,word in [('/Game/ThirdPerson/Maps/Street/BlackMarketStreet','l4BlockAlley'),('/Game/ThirdPerson/Maps/Street/L2StreetOutsideBar','FightingArea')]:
 w=unreal.EditorLoadingAndSavingUtils.load_map(mp);assert w,mp
 for a in unreal.EditorLevelLibrary.get_all_level_actors():
  if word.lower() not in (a.get_name()+' '+a.get_actor_label()+' '+a.get_class().get_name()).lower():continue
  rows=[]
  for c in a.get_components_by_class(unreal.SceneComponent):
   row={'name':c.get_name(),'class':c.get_class().get_name(),'relative':str(c.get_relative_transform()),'world':str(c.get_world_transform())}
   for key in ['static_mesh','override_materials','box_extent','relative_location','relative_rotation','relative_scale3d']:
    try:row[key]=str(c.get_editor_property(key))
    except:pass
   rows.append(row)
  data={'actor':a.get_path_name(),'label':a.get_actor_label(),'transform':str(a.get_actor_transform()),'components':rows}
  (out/(word+'-'+a.get_name()+'.json')).write_text(json.dumps(data,indent=2),encoding='utf8')
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.actor.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
  unreal.log('STYLE_ACTOR '+a.get_path_name())
bp=unreal.load_asset('/Game/Blueprints/BPA_StageBlockingVolume');t.object=bp;t.filename=str(out/'BPA_StageBlockingVolume.copy');unreal.Exporter.run_asset_export_task(t)
