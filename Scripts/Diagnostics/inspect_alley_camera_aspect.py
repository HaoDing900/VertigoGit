import unreal

world = unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/SaApartment/Sa_AptDownAlley')
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    label = actor.get_actor_label()
    if 'CameraLockPoint' not in label and 'Player_Sa' not in label:
        continue
    cameras = actor.get_components_by_class(unreal.CameraComponent)
    for camera in cameras:
        values = {}
        for name in ['aspect_ratio', 'constrain_aspect_ratio', 'field_of_view', 'override_aspect_ratio_axis_constraint', 'aspect_ratio_axis_constraint']:
            try:
                values[name] = str(camera.get_editor_property(name))
            except Exception:
                pass
        unreal.log('ALLEY_CAMERA_ASPECT ' + label + ' ' + actor.get_name() + ' ' + camera.get_path_name() + ' ' + str(values))
unreal.log('ALLEY_CAMERA_ASPECT inspection complete; no assets saved')
