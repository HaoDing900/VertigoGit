import unreal
base='/Game/__VTG_CutsceneAudioTest'
ar=unreal.AssetRegistryHelpers.get_asset_registry()
existing=ar.get_assets_by_path(base,True)
assert all(str(d.asset_name) in ['InstanceTest','ProfileTest'] for d in existing)
for d in existing: assert unreal.EditorAssetLibrary.delete_asset(str(d.package_name))
ss=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);assert ss.new_level(base+'/InstanceTest')
a=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.AmbientSound,unreal.Vector())
b=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.AmbientSound,unreal.Vector(100,0,0))
a.set_actor_label('VTG_Enabled');b.set_actor_label('VTG_DefaultOff')
cue=unreal.load_asset('/Game/SFX/Character/SaFootstep/Cues/Footstep_Boots_01_Cue');assert cue
a.get_component_by_class(unreal.AudioComponent).set_sound(cue);b.get_component_by_class(unreal.AudioComponent).set_sound(cue)
assert not unreal.VTGCutsceneAudioLibrary.get_follow_ambient_sequence_mute(a.get_component_by_class(unreal.AudioComponent))
unreal.VTGCutsceneAudioLibrary.set_follow_ambient_sequence_mute(a.get_component_by_class(unreal.AudioComponent),True)
assert ss.save_current_level()
seq=unreal.AssetToolsHelpers.get_asset_tools().create_asset('ProfileTest',base,unreal.LevelSequence,unreal.LevelSequenceFactoryNew());assert seq
settings=unreal.VTGCutsceneAudioLibrary.get_sequence_audio_settings(seq,True)
settings.set_editor_property('preset',unreal.VTGCutsceneAudioPreset.MUTE_WORLD_BEDS)
assert unreal.EditorAssetLibrary.save_loaded_asset(seq,False)
unreal.log('CUTSCENE_PERSIST_WRITE_PASS')


