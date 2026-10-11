import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/GoonDamageDiagnosis'
paths=['/Game/Characters/NPC/GenericOnes/DrunkBarFightGuy/BP_Enm_BarFighter']
def export(obj,name):
 t=unreal.AssetExportTask();t.object=obj;t.exporter=unreal.ObjectExporterT3D();t.filename=str(out/(name+'.t3d'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
for p in paths:
 b=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=b;t.filename=str(out/(b.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 c=unreal.get_default_object(unreal.load_object(None,p+'.'+p.split('/')[-1]+'_C'))
 lines=[]
 comp=c.get_editor_property('mesh')
 for sock in ['hand_rSocket','hand_lSocket','foot_rSocket','foot_lSocket']:
  bone=comp.get_socket_bone_name(sock);lines.append(str(sock)+' bone='+str(bone)+' index='+str(comp.get_bone_index(bone)))
 (out/(b.get_name()+'.sockets.txt')).write_text('\n'.join(lines),encoding='utf8')
 for handle in unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem).k2_gather_subobject_data_for_blueprint(b):
  o=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
  if o and o.get_class().get_name() in ['BP_AIBehaviorsComponent_C','BP_CollisionComponent_C']:export(o,b.get_name()+'.'+o.get_name())

