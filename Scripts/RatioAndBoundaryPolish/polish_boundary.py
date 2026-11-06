import unreal,shutil
from pathlib import Path
out=Path(unreal.Paths.project_dir())/'Scripts/RatioAndBoundaryPolish';back=out/'Backup';back.mkdir(exist_ok=True)
for rel in ['Content/ThirdPerson/Maps/Street/L2_StreetOutsideBar.umap','Content/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram.uasset']:
 src=Path(unreal.Paths.project_dir())/rel;dest=back/src.name
 if not dest.exists():shutil.copy2(src,dest)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
a=next(a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_class().get_name()=='BP_FightingAreaCollision_BarHologram_C')
boxes={b.get_name():b for b in a.get_components_by_class(unreal.BoxComponent)}
state={n:(b.get_relative_transform(),b.get_unscaled_box_extent()) for n,b in boxes.items()}
old={n:b.get_editor_property('relative_location') for n,b in boxes.items()}
# Move the two boundaries forming the circled corner outward 120 cm; extend their meeting ends.
b=boxes['Box'];v=b.get_editor_property('relative_location');v.y-=120;v.x-=60;b.set_relative_location(v,False,True);s=b.get_editor_property('relative_scale3d');s.x+=120/(b.get_unscaled_box_extent().x*2);b.set_relative_scale3d(s)
b=boxes['Box1'];v=b.get_editor_property('relative_location');v.x-=120;v.y-=60;b.set_relative_location(v,False,True);s=b.get_editor_property('relative_scale3d');s.y+=120/(b.get_unscaled_box_extent().y*2);b.set_relative_scale3d(s)
new={n:(b.get_relative_transform(),b.get_unscaled_box_extent()) for n,b in boxes.items()}
bp=unreal.load_asset('/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram');ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
groups={'Box':[],'Box1':[],'Box2':[]};seen=set()
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 c=lib.get_object(lib.get_data(h))
 if not isinstance(c,unreal.StaticMeshComponent) or 'BattleStyle' not in [str(x) for x in c.get_editor_property('component_tags')] or c.get_path_name() in seen:continue
 seen.add(c.get_path_name());p=c.get_editor_property('relative_location');dist={'Box':abs(p.y-old['Box'].y),'Box1':abs(p.x-old['Box1'].x),'Box2':abs(p.x-old['Box2'].x)};groups[min(dist,key=dist.get)].append(c)
for name,parts in groups.items():
 assert parts;tr,ext=new[name];center=tr.translation;scale=tr.scale3d;along=name=='Box';half=(ext.x*scale.x if along else ext.y*scale.y)
 parts.sort(key=lambda c:c.get_editor_property('relative_location').x if along else c.get_editor_property('relative_location').y)
 for i,c in enumerate(parts):
  offset=(-half+115)+(2*half-230)*i/(len(parts)-1)
  c.set_relative_location(unreal.Vector(center.x+offset if along else center.x,center.y if along else center.y+offset,64.32),False,True)
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
a=next(a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_class().get_name()=='BP_FightingAreaCollision_BarHologram_C')
for b in a.get_components_by_class(unreal.BoxComponent):
 tr,ext=new[b.get_name()];b.modify();b.set_relative_transform(tr,False,True);b.set_box_extent(ext,False)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log('BOUNDARY_POLISH_SAVED outward=120cm two sides corner clearances=115cm totalMeshes='+str(len(seen)))

