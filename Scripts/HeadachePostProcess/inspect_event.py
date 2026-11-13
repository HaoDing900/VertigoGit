from pathlib import Path
import json
import unreal as u
root = Path(u.Paths.project_dir()) / 'Scripts/HeadachePostProcess'
paths = ['/Game/Narrative/Events/CharacterStatus/PP_Sa_Headache', '/Game/Narrative/Events/CharacterStatus/NE_PP_SaHeadache', '/Game/VTG/Blueprints/BP_HeadachePostProcess']
report = {}
for path in paths:
    a = u.load_asset(path)
    assert a
    task = u.AssetExportTask()
    task.object = a
    task.filename = str(root / (a.get_name() + '.copy'))
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    assert u.Exporter.run_asset_export_task(task)
    cdo = u.get_default_object(a.generated_class())
    report[path] = {'class': a.generated_class().get_path_name()}
    for name in ['Duration', 'Intensity', 'Weight', 'HeadacheIntensity', 'BlendWeightIntensity', 'EventRuntime', 'bRefireOnLoad']:
        try: report[path][name] = str(cdo.get_editor_property(name))
        except Exception: pass
sound = u.load_asset('/Game/SFX/Character/SFX_SaHeadache1')
assert sound
report['sound'] = {}
for name in ['duration', 'num_channels', 'sample_rate', 'volume', 'pitch', 'sound_class_object', 'asset_import_data']:
    try: report['sound'][name] = str(sound.get_editor_property(name))
    except Exception: pass
t = u.AssetExportTask()
t.object = sound
t.filename = str(root / 'headache_source.wav')
t.automated = True
t.prompt = False
t.replace_identical = True
report['wav_exported'] = u.Exporter.run_asset_export_task(t)
report['pp_referencers'] = [str(x) for x in u.EditorAssetLibrary.find_package_referencers_for_asset(paths[0])]
(root / 'event-inspection.json').write_text(json.dumps(report, indent=2))
u.log('HEADACHE_EVENT_INSPECT_PASS ' + json.dumps(report))
