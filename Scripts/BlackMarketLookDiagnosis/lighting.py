import unreal

def dump(obj,label):
 unreal.log('LIGHTCHECK '+label+' '+obj.get_path_name())
 for p in ['post_process_blend_weight','post_process_settings','settings','blend_weight','blend_radius','priority','unbound','enabled','current_aperture']:
  try: unreal.log('LIGHTCHECK '+p+'='+str(obj.get_editor_property(p)))
  except:pass
seq=unreal.load_asset('/Game/LevelNSequence/In_Level/L4BlackMarket/PreAdult/L4Market_PreAdult_Intro_SaObserveIPHA')
for b in seq.get_bindings():
 t=b.get_object_template()
 if isinstance(t,unreal.CameraActor):dump(t.get_editor_property('camera_component'),'sequence '+b.get_name())
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.PostProcessVolume):dump(a,'volume')
 if 'BP_Player_Sa' in a.get_class().get_name():
  for c in a.get_components_by_class(unreal.CameraComponent):dump(c,'player camera')


