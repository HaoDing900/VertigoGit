import unreal
p='/Game/LevelNSequence/In_Level/SaApartment/BP_VTG_AudioManager_SaApartment'
if unreal.EditorAssetLibrary.does_asset_exist(p): assert unreal.EditorAssetLibrary.delete_asset(p)
f=unreal.BlueprintFactory(); f.set_editor_property('parent_class',unreal.load_class(None,'/Game/LevelNSequence/LevelAudio/BP_VTG_AudioManager.BP_VTG_AudioManager_C'))
b=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_VTG_AudioManager_SaApartment','/Game/LevelNSequence/In_Level/SaApartment',unreal.Blueprint,f)
unreal.BlueprintEditorLibrary.compile_blueprint(b)
s=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem); lib=unreal.SubobjectDataBlueprintFunctionLibrary
for h in s.k2_gather_subobject_data_for_blueprint(b):
 d=lib.get_data(h); o=lib.get_object(d)
 unreal.log('MED_SUB '+str(o))
 if isinstance(o,unreal.AudioComponent):
  c=lib.get_object_for_blueprint(d,b)
  unreal.log('MED_OVERRIDE '+str(c))
  if c:
   c.set_editor_property('auto_activate',False); c.set_editor_property('sound',None)
unreal.BlueprintEditorLibrary.compile_blueprint(b)
assert unreal.EditorAssetLibrary.save_loaded_asset(b,False)
