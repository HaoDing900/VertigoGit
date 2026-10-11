import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/GoonDamageDiagnosis'
paths=['/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon1Misto','/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon2Nara','/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter']
report=[]
for p in paths:
 cls=unreal.load_object(None,p+'.'+p.split('/')[-1]+'_C')
 a=unreal.EditorLevelLibrary.spawn_actor_from_class(cls,unreal.Vector(0,0,0))
 try:
  mesh=a.get_editor_property('mesh');asset=mesh.get_editor_property('skeletal_mesh_asset');ai=a.get_editor_property('AIBehaviorsComponent');coll=a.get_editor_property('MeleeCollisionComponent')
  r={'asset':p,'mesh':asset.get_path_name(),'skeleton':asset.get_editor_property('skeleton').get_path_name(),'damage':ai.get_editor_property('MeleeAttackDamage'),'attack_distance':ai.get_editor_property('MeleeAttackDistance'),'trace_radius':coll.get_editor_property('TraceRadius'),'sockets':{}}
  for key,bone in [('RightHandSocket','hand_r'),('LeftHandSocket','hand_l'),('RightFootSocket','foot_r')]:
   sock=a.get_editor_property(key);r['sockets'][key]={'name':str(sock),'resolved_bone':str(mesh.get_socket_bone_name(sock)),'actual_bone_index':mesh.get_bone_index(bone),'location':str(mesh.get_socket_location(sock)),'bone_location':str(mesh.get_socket_location(bone))}
  anim=ai.get_editor_property('MeleeAttackAnimation');r['attack_animations']=str(anim)
  # Export animation config and notifies for review, without modifying or playing.
  for montage in anim.get_editor_property('Montage_51_432A1CD448DDF1E103D008A1B0DCC6DE'):
   t=unreal.AssetExportTask();t.object=montage;t.exporter=unreal.ObjectExporterT3D();t.filename=str(out/(montage.get_name()+'.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
  report.append(r)
 finally:unreal.EditorLevelLibrary.destroy_actor(a)
(out/'runtime_config.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('GOON_DAMAGE_CONFIG_PASS')
