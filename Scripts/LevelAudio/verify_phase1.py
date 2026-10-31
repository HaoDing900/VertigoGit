"""Reload the saved Phase 1 map and check the serialized Blueprint/component configuration."""
import json
from pathlib import Path
import unreal

root = '/Game/LevelNSequence/LevelAudio'
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level(root + '/L_VTG_Audio_Test')
bp = unreal.load_asset(root + '/BP_VTG_AudioManager')
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
managers = [a for a in actors if a.get_class() == bp.generated_class()]
assert len(managers) == 1, len(managers)
components = managers[0].get_components_by_class(unreal.AudioComponent)
assert len(components) == 1
audio = components[0]
assert audio.get_name().startswith('AC_Ambience')
sound = audio.get_editor_property('sound')
assert sound.get_path_name() == root + '/SW_VTG_Ambience_Loop.SW_VTG_Ambience_Loop'
assert sound.get_editor_property('looping')
assert audio.get_editor_property('auto_activate')
assert not audio.get_editor_property('allow_spatialization')
assert not audio.get_editor_property('override_attenuation')
assert audio.get_editor_property('attenuation_settings') is None
assert abs(audio.get_editor_property('volume_multiplier') - 0.3) < 0.0001
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property('default_game_mode') == unreal.GameModeBase.static_class()
report = {'saved_assets_verified': True, 'manager_instances': len(managers),
          'audio_component': audio.get_name(), 'looping': True, 'auto_activate': True,
          'sound_duration_seconds': sound.get_editor_property('duration'),
          'listening_test': 'Pending: play L_VTG_Audio_Test in PIE for at least two full loops.'}
(Path(unreal.Paths.project_dir()) / 'Saved/LevelAudioPhase1.json').write_text(
    json.dumps(report, indent=2), encoding='utf-8')
unreal.log('VTG_AUDIO_PHASE1_VERIFIED ' + json.dumps(report))
