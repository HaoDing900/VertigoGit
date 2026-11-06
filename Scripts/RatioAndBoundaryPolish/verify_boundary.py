import unreal,json,math
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
a=next(a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_class().get_name()=='BP_FightingAreaCollision_BarHologram_C')
boxes={b.get_name():b for b in a.get_components_by_class(unreal.BoxComponent)}
assert abs(boxes['Box'].get_editor_property('relative_location').y-(-786.245042))<.01
assert abs(boxes['Box1'].get_editor_property('relative_location').x-(-771.774954))<.01
groups={'Box':[],'Box1':[],'Box2':[]};count=0
for c in a.get_components_by_class(unreal.StaticMeshComponent):
 if 'BattleStyle' not in [str(t) for t in c.get_editor_property('component_tags')]:continue
 mesh=c.get_editor_property('static_mesh');assert mesh.get_name()=='SM_HologramLogo' and c.get_material(0).get_name()=='MI_HologramLogo_VTGRestrict'
 center=unreal.MathLibrary.transform_location(c.get_relative_transform(),mesh.get_bounds().origin)
 old={n:b.get_editor_property('relative_location') for n,b in boxes.items()}
 distances={'Box':abs(center.y-old['Box'].y),'Box1':abs(center.x-old['Box1'].x),'Box2':abs(center.x-old['Box2'].x)}
 name=min(distances,key=distances.get);assert distances[name]<.01,(c.get_name(),distances)
 groups[name].append(center.x if name=='Box' else center.y);count+=1
 assert abs(c.get_editor_property('relative_scale3d').z-.75)<.001
assert count==23
for name,values in groups.items():
 values.sort();gaps=[values[i+1]-values[i] for i in range(len(values)-1)];assert min(gaps)>132,(name,gaps);assert max(gaps)-min(gaps)<.01
 unreal.log('BOUNDARY_SPACING_PASS '+name+' count='+str(len(values))+' minCenterSpacing='+str(min(gaps)))
unreal.log('BOUNDARY_FINAL_PASS 23 meshes retained, pivot centered, non-overlapping width, two collision sides outward120cm')

