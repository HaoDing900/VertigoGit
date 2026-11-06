import unreal
bp=unreal.load_asset('/Game/Blueprints/Actors/FightingAreaCollision/BP_FightingAreaCollision')
ss=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem);lib=unreal.SubobjectDataBlueprintFunctionLibrary
for h in ss.k2_gather_subobject_data_for_blueprint(bp):
 obj=lib.get_object(lib.get_data(h));unreal.log('BARRIER_COMPONENT '+obj.get_name()+' '+obj.get_class().get_name())
 for prop in ['static_mesh','override_materials','asset','child_actor_class','visible','hidden_in_game']:
  try: unreal.log('BARRIER_PROPERTY '+prop+' '+str(obj.get_editor_property(prop)))
  except: pass
