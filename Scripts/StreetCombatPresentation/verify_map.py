import unreal

expected = {'BP_Enm_StreetGoon1Misto_C': 10.0,
            'BP_Enm_StreetGoon2Nara_C': 10.0,
            'BP_Enm_SteetGoon3_Brosky_C': 20.0}
assert unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
found = set()
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    name = actor.get_class().get_name()
    if name not in expected:
        continue
    behavior = actor.get_editor_property('AIBehaviorsComponent')
    assert str(actor.get_editor_property('EnemyDisplayName')) == 'Street Goon'
    assert behavior.get_editor_property('DeadLifespan') == 0.0
    assert behavior.get_editor_property('MeleeAttackDamage') == expected[name]
    animation = behavior.get_editor_property('AnimationOnlyDeath')
    assert bool(animation) == ('Brosky' in name)
    if animation:
        assert animation.get_name() == 'Death_Bwd'
    found.add(name)
    unreal.log('STREET_PRESENTATION_MAP {} name=Street Goon damage={} independent_cleanup=disabled'.format(name, expected[name]))
assert found == set(expected)
unreal.log('STREET_PRESENTATION_MAP_PASS')
