import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
actors=unreal.EditorActorSubsystem().get_all_level_actors()
for a in actors:
 if isinstance(a,unreal.Character) or isinstance(a,unreal.PlayerStart):unreal.log('MAP_ACTOR '+a.get_actor_label()+' '+str(a.get_actor_location()))
 for c in a.get_components_by_class(unreal.StaticMeshComponent):
  mats=c.get_materials()
  if any(m and 'Wall_Concrete1' in m.get_name() for m in mats):unreal.log('MAP_FLOOR '+a.get_actor_label()+' '+str(c.get_world_location())+' visibility='+str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY))+' pawn='+str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)))
