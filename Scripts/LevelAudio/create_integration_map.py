"""Author a separate parent/child audio integration demo using existing licensed project sounds."""
import unreal

ROOT = '/Game/LevelNSequence/LevelAudio'
MAP = ROOT + '/L_VTG_Audio_IntegrationTest'
assert not unreal.EditorAssetLibrary.does_asset_exist(MAP), 'Refusing to overwrite existing test map'
music_path = ROOT + '/SW_VTG_ExampleMusic_Loop'
psych_path = ROOT + '/SW_VTG_ExamplePsych_OneShot'
def copy_sound(source, destination, looping):
    assert not unreal.EditorAssetLibrary.does_asset_exist(destination)
    sound = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    assert isinstance(sound, unreal.SoundWave)
    sound.set_editor_property('looping', looping)
    assert unreal.EditorAssetLibrary.save_loaded_asset(sound)
    return sound

music = copy_sound('/Game/SFX/CinematicAmbientMusicBundle/CyberpunkMusicPack/Wav/02Cyberpunk_Ambient_wav', music_path, True)
# A brief electronic anomaly is a functional placeholder, not final psychological sound design.
psych = copy_sound('/Game/SFX/SciFi_SFX/Audio/UI/rf_pcspeaker_error_2', psych_path, False)
ambience = unreal.load_asset(ROOT + '/SW_VTG_Ambience_Loop')
audio_bp = unreal.load_asset(ROOT + '/BP_VTG_AudioManager_Example')
lm_bp = unreal.load_asset(ROOT + '/BPLM_AudioExample')
assert audio_bp and lm_bp and ambience
defaults = unreal.get_default_object(lm_bp.generated_class())
for key, sound in [('ExampleAmbience', ambience), ('ExampleMusic', music), ('ExamplePsych', psych)]:
    defaults.set_editor_property(key, sound)
assert unreal.EditorAssetLibrary.save_loaded_asset(lm_bp)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.new_level(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
audio = actors.spawn_actor_from_class(audio_bp.generated_class(), unreal.Vector(0, 0, 0))
audio.set_actor_label('Audio Manager - Example Child')
for component in audio.get_components_by_class(unreal.AudioComponent):
    component.set_editor_property('auto_activate', False)
lm = actors.spawn_actor_from_class(lm_bp.generated_class(), unreal.Vector(250, 0, 0))
lm.set_actor_label('BPLM - Audio Stage Example')
lm.set_editor_property('AudioManagerRef', audio)
assert levels.save_current_level()
unreal.log('VTG_AUDIO_INTEGRATION_MAP_SAVED: 0s ambience+music, 6s ambience fade-out+psych, 12s silence')
