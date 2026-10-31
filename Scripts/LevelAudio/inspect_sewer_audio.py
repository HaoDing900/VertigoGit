import unreal
bp = unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_Player_Sa')
sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for h in sub.k2_gather_subobject_data_for_blueprint(bp):
    obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h))
    if obj and obj.get_class().get_name() == 'VTGPlayerAudioComponent':
        for key in ['water_bed', 'water_volume', 'pipe_bed', 'pipe_volume', 'sewer_level']:
            unreal.log('SEWER_AUDIO_SOURCE ' + obj.get_path_name() + ' ' + key + '=' + str(obj.get_editor_property(key)))
