"""Run with UE 5.3 PythonScript commandlet; creates only the isolated Phase 1 assets."""
import json
from pathlib import Path
import unreal

ROOT = '/Game/LevelNSequence/LevelAudio'
REPORT = Path(unreal.Paths.project_dir()) / 'Saved/LevelAudioPhase1.json'
paths = [ROOT + '/' + name for name in (
    'SW_VTG_Ambience_Loop', 'BP_VTG_AudioManager', 'L_VTG_Audio_Test')]
for path in paths[1:]:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Refusing to overwrite existing asset: ' + path)

sound = unreal.load_asset(paths[0]) if unreal.EditorAssetLibrary.does_asset_exist(paths[0]) else unreal.EditorAssetLibrary.duplicate_asset(
    '/Game/SFX/SciFi_SFX/Audio/Engine_hum/engine_background_hum_pipes', paths[0])
assert isinstance(sound, unreal.SoundWave)
sound.set_editor_property('looping', True)
assert unreal.EditorAssetLibrary.save_loaded_asset(sound)

factory = unreal.BlueprintFactory()
factory.set_editor_property('parent_class', unreal.Actor)
bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    'BP_VTG_AudioManager', ROOT, unreal.Blueprint, factory)
assert bp
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
handle, reason = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(
    parent_handle=handles[0], new_class=unreal.AudioComponent, blueprint_context=bp))
assert subsystem.rename_subobject(handle, 'AC_Ambience'), str(reason)
data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
audio = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
assert isinstance(audio, unreal.AudioComponent)
for key, value in {
    'sound': sound, 'auto_activate': True, 'volume_multiplier': 0.3,
    'pitch_multiplier': 1.0, 'allow_spatialization': False,
    'attenuation_settings': None, 'override_attenuation': False,
}.items():
    audio.set_editor_property(key, value)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
assert unreal.EditorAssetLibrary.save_loaded_asset(bp)

levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.new_level(paths[2])
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
instance = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(0, 0, 0))
assert instance
instance.set_actor_label('BP_VTG_AudioManager')
component = instance.get_component_by_class(unreal.AudioComponent)
assert component.get_editor_property('sound') == sound
assert component.get_editor_property('auto_activate')
assert not component.get_editor_property('allow_spatialization')
assert sound.get_editor_property('looping')
assert levels.save_current_level()
report = {
    'assets': paths, 'looping': True, 'auto_activate': True,
    'audio_component': component.get_name(), 'volume_multiplier': 0.3,
    'generated_class': bp.generated_class().get_path_name(),
    'validation': 'Blueprint compiled and placed instance properties checked. Listening in PIE still required.',
}
REPORT.write_text(json.dumps(report, indent=2), encoding='utf-8')
unreal.log('VTG_AUDIO_PHASE1_SUCCESS ' + json.dumps(report))
