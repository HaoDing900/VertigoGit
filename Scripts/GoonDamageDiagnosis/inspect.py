import unreal,json,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/GoonDamageDiagnosis'
paths=['/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon1Misto','/Game/Characters/NPC/StreetGoons/BP_Enm_StreetGoon2Nara','/Game/Characters/NPC/StreetGoons/BP_Enm_SteetGoon3_Brosky','/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_CollisionComponent','/Game/AI/AIBehaviorSystem/Blueprint/Component/BP_AIBehaviorsComponent','/Game/AI/AIBehaviorSystem/Blueprint/AI/BP_AI_Base']
for p in paths:
 b=unreal.load_asset(p)
 c=unreal.get_default_object(unreal.load_object(None,p+'.'+p.split('/')[-1]+'_C'))
 task=unreal.AssetExportTask();task.object=c;task.exporter=unreal.ObjectExporterT3D();task.filename=str(out/(b.get_name()+'.defaults.t3d'));task.automated=True;task.prompt=False
 unreal.Exporter.run_asset_export_task(task)
 lines=['ASSET '+p,'CDO '+str(c)]
 for name in dir(c):
  try:
   v=c.get_editor_property(name)
   lines.append(name+' = '+str(v))
  except: pass
 if not isinstance(c,unreal.Actor):
  task=unreal.AssetExportTask();task.object=b;task.filename=str(out/(b.get_name()+'.copy'));task.automated=True;task.prompt=False;unreal.Exporter.run_asset_export_task(task)
  continue
 sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
 handles=sub.k2_gather_subobject_data_for_blueprint(b)
 for handle in handles:
  data=unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
  o=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
  lines.append('OBJECT '+o.get_path_name()+' '+o.get_class().get_name())
  if isinstance(o,unreal.ActorComponent):
   for name in dir(o):
    try:lines.append('  '+name+' = '+str(o.get_editor_property(name)))
    except:pass
 (out/(b.get_name()+'.txt')).write_text('\n'.join(lines),encoding='utf8')
 task=unreal.AssetExportTask();task.object=b;task.filename=str(out/(b.get_name()+'.copy'));task.automated=True;task.prompt=False
 try:unreal.Exporter.run_asset_export_task(task)
 except Exception as e:unreal.log(str(e))
unreal.log('WEAPON_DIAG_DONE')


