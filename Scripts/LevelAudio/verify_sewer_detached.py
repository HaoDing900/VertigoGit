import unreal
bp = unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_Player_Sa')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
found = False
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if obj and obj.get_class().get_name() == 'VTGPlayerAudioComponent':
        found = True
        # Sewer fields are intentionally no longer editor/Python exposed.
        # Native serialized values are separately checked by VTGLevelAudio -SewerDetachedCheck.
        for key in ['water_bed', 'pipe_bed', 'sewer_level']:
            try:
                obj.get_editor_property(key)
            except Exception:
                pass
            else:
                raise AssertionError('Legacy sewer property is still editor exposed: ' + key)
        assert len(obj.get_editor_property('default_steps')) > 0
        assert len(obj.get_editor_property('armor_run_start_sounds')) > 0
        unreal.log('VTG_SEWER_UI_VERIFIED: sewer fields hidden; footsteps and armor sources retained')
assert found
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
