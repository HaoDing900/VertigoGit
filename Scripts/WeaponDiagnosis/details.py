import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/WeaponDiagnosis'
paths=['/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky','/Game/AI/AIBehaviorSystem/Blueprint/AI/Example/BP_AI_SoldierMelee']
for p in paths:
 b=unreal.load_asset(p);c=unreal.get_default_object(unreal.load_object(None,p+'.'+p.split('/')[-1]+'_C'))
 task=unreal.AssetExportTask();task.object=c;task.exporter=unreal.ObjectExporterT3D();task.filename=str(out/(b.get_name()+'.defaults.t3d'));task.automated=True;task.prompt=False
 unreal.Exporter.run_asset_export_task(task)
 lines=[]
 for name in ['WeaponEquipSocket','WeaponUnequipSocket','Behavior','BehaviorTree','AIBehavior','Weapon','Mesh','AttackMontage','EquipMontage']:
  try:lines.append(name+'='+str(c.get_editor_property(name)))
  except Exception as e:lines.append(name+':'+str(e))
 mesh=c.get_editor_property('mesh');asset=mesh.get_editor_property('skeletal_mesh_asset')
 lines.extend(['SKELETON '+str(asset.get_editor_property('skeleton')),'SOCKETS '+str(mesh.get_all_socket_names())])
 for bone in ['weapon_r','weapon_l','hand_r','spine_03']:
  lines.append('BONE_INDEX '+bone+'='+str(mesh.get_bone_index(bone)))
 for sock in ['baseballbat_hand','baseballbat_back']:
  lines.append('SOCKET_BONE '+sock+'='+str(mesh.get_socket_bone_name(sock)))
 for name in mesh.get_all_socket_names():
  if 'weapon' in str(name).lower() or 'bat' in str(name).lower(): lines.append(str(name)+' '+str(mesh.get_socket_transform(name,unreal.RelativeTransformSpace.RTS_COMPONENT)))
 (out/(b.get_name()+'.sockets.txt')).write_text('\n'.join(lines),encoding='utf8')

