import unreal
from pathlib import Path
root = Path(unreal.Paths.project_dir()) / 'Content/SFX/SaArmor'
tasks = []
for name in ['360694__postworkflow__janglingkeys-sauer'] + ['SW_SaArmor_RunStart_%02d' % i for i in range(1, 4)]:
    t = unreal.AssetImportTask()
    t.filename = str(root / (name + '.wav'))
    t.destination_path = '/Game/SFX/SaArmor'
    t.automated = True
    t.save = True
    t.replace_existing = True
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for t in tasks:
    assert t.imported_object_paths, t.filename
    wave = unreal.load_asset(t.imported_object_paths[0])
    assert isinstance(wave, unreal.SoundWave)
    assert not wave.get_editor_property('looping')
    unreal.log('ARMOR IMPORT ' + wave.get_path_name())

