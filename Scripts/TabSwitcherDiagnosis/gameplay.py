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
 global stage,deadline
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
   stage=3;deadline=time.monotonic()+1;return
  if stage==3:
   finish()
 except Exception:
  log({'error':traceback.format_exc(),'stage':stage});finish()
log({'api':'start'})
unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/BaseLevel')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'VTG.TabSwitcherStartPIE')
