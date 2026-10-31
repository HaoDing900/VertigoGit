"""Read-only reload checks plus compilation of existing BPLM children; saves no assets."""
import unreal
ROOT = '/Game/LevelNSequence/LevelAudio'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(ROOT + '/L_VTG_Audio_IntegrationTest')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
audio_class = unreal.load_asset(ROOT + '/BP_VTG_AudioManager_Example').generated_class()
lm_class = unreal.load_asset(ROOT + '/BPLM_AudioExample').generated_class()
audio = next(a for a in actors if a.get_class() == audio_class)
lm = next(a for a in actors if a.get_class() == lm_class)
assert lm.get_editor_property('AudioManagerRef') == audio
assert len(audio.get_components_by_class(unreal.AudioComponent)) == 3
assert all(not c.get_editor_property('auto_activate') for c in audio.get_components_by_class(unreal.AudioComponent))
assert lm.get_editor_property('ExampleAmbience').get_editor_property('looping')
assert lm.get_editor_property('ExampleMusic').get_editor_property('looping')
assert not lm.get_editor_property('ExamplePsych').get_editor_property('looping')
registry = unreal.AssetRegistryHelpers.get_asset_registry()
count = 0
for data in registry.get_assets_by_path('/Game/LevelNSequence', recursive=True):
    if str(data.asset_name).startswith('BPLM') and str(data.asset_class_path.asset_name) == 'Blueprint':
        bp = data.get_asset()
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        count += 1
        unreal.log('VTG_AUDIO_REGRESSION_COMPILED ' + bp.get_path_name())
unreal.log('VTG_AUDIO_MAP_VERIFIED existing_BPLM_blueprints_compiled=' + str(count))
