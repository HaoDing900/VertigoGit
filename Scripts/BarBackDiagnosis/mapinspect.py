import unreal
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L2Bar')
for a in unreal.EditorActorSubsystem().get_all_level_actors():
 if 'Cine_Sa' in a.get_class().get_name() or 'BarBack' in a.get_class().get_name():
  unreal.log('BARCHECK '+a.get_path_name()+' tags='+str(a.tags))
  for m in a.get_components_by_class(unreal.SkeletalMeshComponent):
   unreal.log('BARCHECK MESH '+m.get_name()+' mode='+str(m.get_editor_property('animation_mode'))+' class='+str(m.get_editor_property('anim_class')))
  if 'BarBack' in a.get_class().get_name():
   try: unreal.log('BARCHECK PLAYER '+str(a.get_editor_property('Player Cinematic')))
   except Exception as e: unreal.log(str(e))
