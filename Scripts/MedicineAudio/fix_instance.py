import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/SaApartment')
lm=next(a for a in unreal.EditorLevelLibrary.get_all_level_actors() if a.get_class().get_name()=='BPLM_SaApartment_C')
a=lm.get_editor_property('AudioManagerRef'); assert a
a.modify()
for c in a.get_components_by_class(unreal.AudioComponent):
 unreal.log('MED_BEFORE '+c.get_name()+' auto='+str(c.get_editor_property('auto_activate'))+' sound='+str(c.get_editor_property('sound')))
 c.modify(); c.set_editor_property('auto_activate',False); c.set_editor_property('sound',None)
assert unreal.EditorLoadingAndSavingUtils.save_current_level()
# Remove unused temporary blueprint created during diagnosis.
assert unreal.EditorAssetLibrary.delete_asset('/Game/LevelNSequence/In_Level/SaApartment/BP_VTG_AudioManager_SaApartment')
unreal.log('MED_INSTANCE_SAVED')
