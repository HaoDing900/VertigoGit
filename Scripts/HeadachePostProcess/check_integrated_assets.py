"""Read-only inspection of saved playback assets and the old BaseLevel reference."""
import json
from pathlib import Path
import unreal as u
root = Path(u.Paths.project_dir()) / 'Scripts/HeadachePostProcess'
bp = u.load_asset('/Game/Narrative/Events/CharacterStatus/PP_Sa_Headache')
event_bp = u.load_asset('/Game/Narrative/Events/CharacterStatus/NE_PP_SaHeadache')
effect = u.get_default_object(bp.generated_class())
event = u.get_default_object(event_bp.generated_class())
report = {'event': event_bp.get_path_name(), 'effect': bp.get_path_name(), 'event_defaults': {}}
for name in ['Blend Weight Intensity', 'HeadacheDuration', 'HeadacheAudioVolume', 'HeadacheEnvelopeInfluence']:
    report['event_defaults'][name] = event.get_editor_property(name)
assert report['event_defaults']['Blend Weight Intensity'] == 1.0
assert abs(report['event_defaults']['HeadacheDuration'] - 5.0) < 0.0001
assert effect.headache_material.get_path_name().endswith('M_PP_Headache.M_PP_Headache')
assert effect.headache_sound.get_path_name().endswith('SFX_SaHeadache1.SFX_SaHeadache1')
report['material'] = effect.headache_material.get_path_name()
report['sound'] = effect.headache_sound.get_path_name()
u.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/BaseLevel')
report['base_level_instances'] = []
for actor in u.get_editor_subsystem(u.EditorActorSubsystem).get_all_level_actors():
    if isinstance(actor, u.VTGSaHeadacheEffect):
        pp = actor.get_components_by_class(u.PostProcessComponent)
        audio = actor.get_components_by_class(u.AudioComponent)
        assert len(pp) == 1 and len(audio) == 1
        assert pp[0].get_editor_property('blend_weight') == 0.0
        report['base_level_instances'].append({'actor': actor.get_path_name(), 'pp_count': len(pp), 'audio_count': len(audio)})
(root / 'narrative-result.json').write_text(json.dumps(report, indent=2))
u.log('HEADACHE_SAVED_DEFAULTS_AND_BASELEVEL_PASS ' + json.dumps(report))
