import unreal,time,pathlib,json,traceback
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/TabSwitcherDiagnosis/gameplay-runtime.json'
report={'events':[],'scope':'normal PIE in BaseLevel with actual player; Tab input through player controller and Q/E through Slate'};start=time.monotonic();stage=0;deadline=0

def log(v):
 report['events'].append(v);out.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8');unreal.log('TABPROBE '+str(v))
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorLevelLibrary.editor_end_play()
 unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'QUIT_EDITOR')
def tick(delta):
 global stage,deadline,visible_root
 try:
  if time.monotonic()-start>150:log({'timeout':stage});finish();return
  w=unreal.EditorLevelLibrary.get_game_world()
  if not w or time.monotonic()<deadline:return
  if stage==0:
   pc=unreal.GameplayStatics.get_player_controller(w,0)
   pawn=unreal.GameplayStatics.get_player_pawn(w,0)
   log({'world':w.get_name(),'pc':str(pc),'pawn':str(pawn)})
   if pawn:
    for n in ['bIsActionMode','AC_Inventory_HUD_Component']:
     try:log({n:str(pawn.get_editor_property(n))})
     except Exception as e:log({n:str(e)})
   stage=1;deadline=time.monotonic()+3;return
  if stage==1:
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherPressTab')
   stage=2;deadline=time.monotonic()+1;return
  if stage==2:
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherPressTab release')
   widgets=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,False)
   log({'widgets':[{'name':x.get_name(),'class':x.get_class().get_name(),'visible':x.is_visible(),'focus':x.has_keyboard_focus(),'descendant_focus':x.has_focused_descendants()} for x in widgets if 'Tab' in x.get_class().get_name() or x.has_keyboard_focus()]})
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe')
   roots=[x for x in widgets if x.get_class().get_name()=='WB_TabSwitcher_C']
   for x in roots:
    data={'root':x.get_path_name(),'visible':x.is_visible(),'in_viewport':x.is_in_viewport()}
    for n in ['Tab Switcher Widget','HUD_InventoryComponent_Reference']:
     try:data[n]=str(x.get_editor_property(n))
     except Exception as e:data[n]=str(e)
    log(data)
   visible_root=[x for x in roots if x.is_visible()][0]
   for actor in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
    for c in actor.get_components_by_class(unreal.WidgetComponent):
     data={'actor':actor.get_name(),'component':c.get_name(),'widget':str(c.get_user_widget_object())}
     for n in ['receive_hardware_input','window_focusable','space']:
      try:data[n]=str(c.get_editor_property(n))
      except Exception as e:data[n]=str(e)
     log(data)
   stage=3;deadline=time.monotonic()+1;return
  if stage==3:
   visible_root.set_keyboard_focus()
   log({'after_manual_focus':visible_root.has_keyboard_focus(),'descendant_focus':visible_root.has_focused_descendants()})
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe')
   menu=visible_root.call_method('GetTabSwitcherMenu')
   key=unreal.Key();key.import_text('E')
   visible_root.call_method('TabSwitcherInputs',(key,))
   log({'after_direct_E':str(menu.call_method('GetActiveTabIndex'))})
   finish()
 except Exception:
  log({'error':traceback.format_exc(),'stage':stage});finish()
log({'api':'start'})
for p in ['/Game/ThirdPerson/Blueprints/BP_Player_Sa','/Game/Characters/Sa/PortableTerminal/BP_PortableTerminal','/Game/ThirdPerson/PC_Game_VTG']:
 a=unreal.load_asset(p)
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out.parent/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/BaseLevel')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'VTG.TabSwitcherStartPIE')

