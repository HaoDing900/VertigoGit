import unreal
for path in ['/Game/ThirdPerson/Maps/BaseLevel','/Game/ThirdPerson/Maps/Street/BlackMarketStreet']:
 unreal.EditorLoadingAndSavingUtils.load_map(path)
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
 unreal.log('BASEPP MAP '+path)
 for a in actors:
  if isinstance(a,unreal.PostProcessVolume):
   unreal.log('BASEPP VOLUME '+a.get_actor_label())
   for p in ['enabled','unbound','blend_weight','priority']:unreal.log('BASEPP '+p+'='+str(a.get_editor_property(p)))
   s=a.get_editor_property('settings')
   for p in ['auto_exposure_method','auto_exposure_bias','auto_exposure_apply_physical_camera_exposure','auto_exposure_min_brightness','auto_exposure_max_brightness','depth_of_field_fstop','camera_iso','camera_shutter_speed','color_gain','color_gamma','color_saturation','film_slope','vignette_intensity','bloom_intensity']:
    unreal.log('BASEPP '+p+'='+str(s.get_editor_property(p))+' override='+str(s.get_editor_property('override_'+p)))
