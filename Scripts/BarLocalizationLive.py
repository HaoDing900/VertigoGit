import unreal,time,traceback
state=0; deadline=0; start=time.monotonic()
def tick(dt):
 global state,deadline
 if time.monotonic()<deadline:return
 try:
  w=unreal.EditorLevelLibrary.get_game_world()
  if not w:
   if time.monotonic()-start>100:finish()
   return
  if state==0:state=1;deadline=time.monotonic()+4;return
  commands=['VTG.BarLocalizationProbe start','VTG.BarLocalizationProbe inspect','vtg.Language zh','VTG.BarLocalizationProbe inspect','vtg.Language en','VTG.BarLocalizationProbe inspect']
  if state<=len(commands):
   unreal.SystemLibrary.execute_console_command(w,commands[state-1]);state+=1;deadline=time.monotonic()+1;return
  finish()
 except Exception:
  unreal.log_error(traceback.format_exc());finish()
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorLevelLibrary.editor_end_play()
 unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'QUIT_EDITOR')
unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/L2Bar')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'VTG.TabSwitcherStartPIE')
