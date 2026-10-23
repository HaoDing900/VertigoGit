import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/Street/BlackMarketStreet')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors();sa=next(a for a in actors if a.get_class().get_name()=='BP_Player_Sa_C')
capsule=sa.get_component_by_class(unreal.CapsuleComponent);radius=capsule.get_scaled_capsule_radius();half=capsule.get_scaled_capsule_half_height()
unreal.log('OVERLAP227 capsule radius='+str(radius)+' half='+str(half))
for label,pos in [('target',unreal.Vector(361.971893,4234.076172,255.6595)),('observed',unreal.Vector(318.644,4040.297,249.206))]:
 comps=unreal.SystemLibrary.capsule_overlap_components(w,pos,radius,half,[unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY1,unreal.ObjectTypeQuery.OBJECT_TYPE_QUERY2],unreal.PrimitiveComponent,[sa])
 unreal.log('OVERLAP227 '+label+' count='+str(len(comps or [])))
 for c in comps or []:
  unreal.log('OVERLAP227 '+label+' '+c.get_owner().get_actor_label()+' '+c.get_path_name()+' enabled='+str(c.get_collision_enabled())+' pawn='+str(c.get_collision_response_to_channel(unreal.CollisionChannel.PAWN)))

