import unreal
p='/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram';bp=unreal.load_asset(p);ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary;seen=set()
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 c=lib.get_object(lib.get_data(h))
 if isinstance(c,unreal.StaticMeshComponent) and 'BattleStyle' in [str(t) for t in c.get_editor_property('component_tags')] and c.get_path_name() not in seen:
  seen.add(c.get_path_name());center=c.get_editor_property('relative_location');c.set_relative_scale3d(unreal.Vector(1,1,.9));tr=c.get_relative_transform();origin=c.get_editor_property('static_mesh').get_bounds().origin;offset=unreal.MathLibrary.transform_location(tr,origin)-tr.translation;c.set_relative_location(center-offset,False,True)
assert len(seen)==23
unreal.BlueprintEditorLibrary.compile_blueprint(bp);assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.log('STYLE_SPACING_SCALE_PASS 23 original meshes, width90percent, height unchanged')

