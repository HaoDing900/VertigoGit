import pathlib
import shutil
import unreal

root = pathlib.Path(unreal.Paths.project_dir())
backup = root / 'Scripts/StreetFlowFix/BackupDamageRetune'
backup.mkdir(exist_ok=True)
targets = {'BP_Enm_StreetGoon1Misto': 10.0,
           'BP_Enm_StreetGoon2Nara': 10.0,
           'BP_Enm_SteetGoon3_Brosky': 20.0}
verify = '-DamageVerify' in unreal.SystemLibrary.get_command_line()
for name, value in targets.items():
    path = '/Game/Characters/NPC/StreetGoons/' + name
    bp = unreal.load_asset(path)
    assert bp
    component = unreal.load_object(None, path + '.' + name + '_C:AIBehaviorsComponent_GEN_VARIABLE')
    assert component and component.get_class().get_name() == 'BP_AIBehaviorsComponent_C'
    # Each child already has its own override. Never edit the shared parent template.
    assert name in component.get_path_name(), component.get_path_name()
    death = component.get_editor_property('AnimationOnlyDeath')
    before = component.get_editor_property('MeleeAttackDamage')
    if not verify:
        src = root / 'Content' / (path[6:] + '.uasset')
        dst = backup / src.name
        if not dst.exists():
            shutil.copy2(src, dst)
        component.set_editor_property('MeleeAttackDamage', value)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assert unreal.EditorAssetLibrary.save_loaded_asset(bp, False)
    assert component.get_editor_property('MeleeAttackDamage') == value
    assert component.get_editor_property('AnimationOnlyDeath') == death
    unreal.log('DAMAGE_RETUNE {} {} -> {}'.format(name, before, value))

assert unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
found = set()
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    name = actor.get_class().get_name().removesuffix('_C')
    if name in targets:
        component = actor.get_editor_property('AIBehaviorsComponent')
        assert component.get_editor_property('MeleeAttackDamage') == targets[name]
        found.add(name)
        unreal.log('DAMAGE_MAP_VERIFIED {} {}'.format(name, targets[name]))
assert found == set(targets), found
unreal.log('DAMAGE_RETUNE_PASS')
