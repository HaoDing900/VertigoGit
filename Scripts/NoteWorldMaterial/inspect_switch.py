import unreal,json
unreal.EditorLoadingAndSavingUtils.load_map('/Game/ThirdPerson/Maps/L_SewerUnderApartment')
rows=[]
gi=unreal.get_default_object(unreal.load_class(None,'/Game/ThirdPerson/GameInstance/BP_VTG_GameInstance.BP_VTG_GameInstance_C'))
rows.append({'game_instance_default_stage':str(gi.get_editor_property('CurrentStage'))})
for a in unreal.EditorLevelLibrary.get_all_level_actors():
 if a.get_actor_label()=='BP_0ISXInteractiveCollision_L3Switch':
  r={'name':a.get_path_name(),'location':str(a.get_actor_location())}
  for p in ['CanInteract?','ActiveStage?','TRIGGER ID','DoOnce?','CallLevelManager?','Call Level Manager?']:
   try:r[p]=str(a.get_editor_property(p))
   except:pass
  r['components']=[]
  for c in a.get_components_by_class(unreal.ActorComponent):
   cr={'name':c.get_name(),'class':c.get_class().get_path_name()}
   if isinstance(c,unreal.PrimitiveComponent):
    cr.update({'location':str(c.get_world_location()),'scale':str(c.get_world_scale()),'generate_overlap':c.get_editor_property('generate_overlap_events'),'collision':str(c.get_collision_enabled()),'object_type':str(c.get_collision_object_type()),'profile':str(c.get_collision_profile_name()),'visibility':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_VISIBILITY)),'camera':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_CAMERA)),'pawn_response':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN))})
   for p in ['ShowInteractionIcon','WB Interact Widget','IsInteractionEnabled','InteractionIconOffset','OverrideDetectionDistance','OverridePickUpDistance','ShowWidgetAtStart','InteractionEnabled','Interaction Enabled','InteractionDistance','Interaction Distance','Interaction Text','UseInteractionDistance','Interaction Type','InteractionType','TraceChannel','CollisionBox','InteractionCollision']:
    try:cr[p]=str(c.get_editor_property(p))
    except:pass
   if c.get_name()=="Cube(Don't Change)": isx_channel=c.get_collision_object_type()
   if isinstance(c,unreal.BoxComponent): cr['extent']=str(c.get_scaled_box_extent())
   r['components'].append(cr)
  rows.append(r)
  a.set_actor_location(unreal.Vector(620.597948,16201.361817,-692.8291),False,False)
  r['traces']=[]
  for x,y in [(150,0),(-150,0),(0,150),(0,-150),(150,-150),(-150,-150),(300,0),(0,300),(0,-300)]:
   end=a.get_actor_location();start=end+unreal.Vector(x,y,80)
   hit=unreal.SystemLibrary.line_trace_single(a,start,end,unreal.TraceTypeQuery.TRACE_TYPE_QUERY1,False,[],unreal.DrawDebugTrace.NONE,False)
   parts=hit.to_tuple()
   r['traces'].append({'offset':[x,y,80],'blocking':parts[0],'actors':[v.get_name() for v in parts if isinstance(v,unreal.Actor)]})
player=unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.load_class(None,'/Game/ThirdPerson/Blueprints/BP_Player_Sa.BP_Player_Sa_C'),unreal.Vector(620,16400,-675))
if player:
 for c in player.get_components_by_class(unreal.PrimitiveComponent):
  if c.get_name()=='InventoryXCollision':
   rows.append({'player_detector':c.get_name(),'collision':str(c.get_collision_enabled()),'generate_overlap':c.get_editor_property('generate_overlap_events'),'ISX_response':str(c.get_collision_response_to_channel(isx_channel)),'dynamic_response':str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_WORLD_DYNAMIC))})
 unreal.EditorLevelLibrary.destroy_actor(player)
open(unreal.Paths.project_dir()+'Scripts/NoteWorldMaterial/switch-interaction.json','w').write(json.dumps(rows,indent=2))
for path in ['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/Blueprints/BP_0ISXInteractiveCollision','/InventorySystemX/InventorySystemX/Components/AC_ActorInteractionComponent','/InventorySystemX/InventorySystemX/Components/AC_PlayerInteractionComponent']:
 a=unreal.load_asset(path);t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/NoteWorldMaterial/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True;unreal.Exporter.run_asset_export_task(t)








