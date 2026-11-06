import unreal
factory=unreal.BlueprintFactory(); factory.set_editor_property('parent_class',unreal.load_class(None,'/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager.BP_VTG_AudioManager_C'))
bp=unreal.EditorAssetLibrary.duplicate_asset('/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager','/Game/LevelNSequence/In_Level/SaApartment/BP_VTG_AudioManager_SaApartment'); assert bp
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem); lib=unreal.SubobjectDataBlueprintFunctionLibrary
count=0
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 d=lib.get_data(h); obj=lib.get_object(d)
 if isinstance(obj,unreal.AudioComponent):
  c=obj; assert c
  c.set_editor_property('auto_activate',False); c.set_editor_property('sound',None); count+=1
assert count==3
unreal.BlueprintEditorLibrary.compile_blueprint(bp); assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/SaApartment')
lm=next(a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_class().get_name()=='BPLM_SaApartment_C')
old=lm.get_editor_property('AudioManagerRef'); assert old
new=unreal.EditorLevelLibrary.spawn_actor_from_class(bp.generated_class(),old.get_actor_location()); assert new
new.set_actor_label('BP_VTG_AudioManager_SaApartment')
lm.set_editor_property('AudioManagerRef',new)
assert unreal.EditorLevelLibrary.destroy_actor(old)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
unreal.log('MED_CHILD_SAVED')

