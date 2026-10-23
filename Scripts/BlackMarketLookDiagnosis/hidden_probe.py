import unreal
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
m=next(a for a in actors if a.get_class().get_name()=='BPLM_BlackMarket_C')
sa=next(a for a in actors if a.get_class().get_name()=='BP_Player_Sa_C')
for a in actors:
 if any(s in a.get_class().get_name() for s in ['BP_Player_Sa','BP_Cine_Sa']) or any(str(t) in ['Player','Sa','SaP'] for t in a.tags):
  unreal.log('HIDECHECK actor='+a.get_path_name()+' tags='+str(a.tags)+' hidden='+str(a.get_editor_property('hidden')))
mesh=sa.get_editor_property('Mesh')
def status(label):
 unreal.log('HIDECHECK '+label+' actorhidden='+str(sa.get_editor_property('hidden'))+' meshvisible='+str(mesh.is_visible())+' meshhidden='+str(mesh.get_editor_property('hidden_in_game'))+' actorlocation='+str(sa.get_actor_location())+' meshlocation='+str(mesh.get_world_location()))
m.set_editor_property('Player Pawn',sa)
status('initial')
sa.call_method('TurnInvisibale');status('after TurnInvisibale')
m.call_method('L4_Position_SaIPHAInfrontOfAdultStore');status('after positioning')
sa.call_method('TurnVisible');status('after TurnVisible')
