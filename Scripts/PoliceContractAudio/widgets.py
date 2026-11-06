import unreal
for name,path in [('NoteWidget','/Game/Inventory/InventoryXBased/UMG/WB_NoteFullScreen'),('NoteMain','/Game/Inventory/InventoryXBased/UMG/WB_NoteMainWidget')]:
 a=unreal.load_asset(path);t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/PoliceContractAudio/'+name+'.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
w=unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/PoliceStation/L3_CentralPoliceStation_FrontDoor_DownStream')
point=unreal.Vector(1018.154541,1596.237183,249.582504)
for a in unreal.EditorActorSubsystem().get_all_level_actors():
 if any(k in a.get_class().get_name() for k in ['Trigger','BPLM']):
  origin,ext=a.get_actor_bounds(False)
  if abs(origin.x-point.x)<ext.x+100 and abs(origin.y-point.y)<ext.y+100 and abs(origin.z-point.z)<ext.z+100:
   unreal.log('CONTRACT_NEAR '+a.get_actor_label()+' class='+a.get_class().get_path_name()+' center='+str(origin)+' extent='+str(ext))
