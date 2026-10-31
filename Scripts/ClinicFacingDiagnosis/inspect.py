import unreal
a=unreal.load_asset('/Game/ThirdPerson/Blueprints/BP_Player_Sa')
c=unreal.get_default_object(a.generated_class())
for obj,names in [(c,['use_controller_rotation_yaw','use_controller_rotation_pitch','use_controller_rotation_roll']),(c.character_movement,['orient_rotation_to_movement','use_controller_desired_rotation','rotation_rate']),(c.mesh,['relative_rotation'])]:
 for n in names:
  unreal.log('CLINIC FACING '+n+'='+str(obj.get_editor_property(n)))
for p in ['/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_Still','/Game/Narrative/Dialogues/Levels/L5Clinic/L5Clinic_FreeMove']:
 d=unreal.get_default_object(unreal.load_asset(p).generated_class())
 for n in ['auto_rotate_speakers','adjust_player_transform','free_movement','auto_stop_movement']:
  try:unreal.log('CLINIC FACING '+p+' '+n+'='+str(d.get_editor_property(n)))
  except Exception as e:unreal.log(str(e))
