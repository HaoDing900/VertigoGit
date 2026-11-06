import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/L2_StreetOutsideBar')
actors=unreal.EditorLevelLibrary.get_all_level_actors();old=next(a for a in actors if a.get_class().get_name()=='BP_FightingAreaCollision_C');lm=next(a for a in actors if a.get_class().get_name()=='BPLM_L2StreetBarFight_C')
assert lm.get_editor_property('FightAreaCollision')==old
bp=unreal.load_asset('/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision_BarHologram');assert bp
new=unreal.EditorLevelLibrary.spawn_actor_from_class(bp.generated_class(),old.get_actor_location(),old.get_actor_rotation());new.set_actor_scale3d(old.get_actor_scale3d());new.set_actor_label('BP_FightingAreaCollision_BarHologram')
for source in old.get_components_by_class(unreal.SceneComponent):
 dest=next((c for c in new.get_components_by_class(unreal.SceneComponent) if c.get_name()==source.get_name()),None)
 if dest:
  dest.modify();dest.set_relative_transform(source.get_relative_transform(),False,True)
  if isinstance(source,unreal.BoxComponent):dest.set_box_extent(source.get_unscaled_box_extent(),False)
lm.set_editor_property('FightAreaCollision',new)
assert unreal.EditorLevelLibrary.destroy_actor(old)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log('BOUNDARY_MAP_SAVED')
