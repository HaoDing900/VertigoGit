import unreal,time,pathlib,json,traceback
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/TabSwitcherDiagnosis/continuous-baseline.json'
report={'events':[],'failures':0,'scope':'BaseLevel/Sa; each Q/E uses native Slate input on a separate frame; initial focus set once only'};start=time.monotonic();stage=0;deadline=0;step=0
keys=['Q']*12+['E']*12+['Q','E','Q','E']
def log(v):
 report['events'].append(v);out.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8');unreal.log('TAB_CONTINUOUS '+str(v))
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorLevelLibrary.editor_end_play()
 unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'QUIT_EDITOR')
def tick(delta):
 global stage,deadline,step,root,menu,expected
 try:
  if time.monotonic()-start>150:log({'timeout':stage});finish();return
  w=unreal.EditorLevelLibrary.get_game_world()
  if not w or time.monotonic()<deadline:return
  if stage==0:
   log({'world':w.get_name(),'pawn':str(unreal.GameplayStatics.get_player_pawn(w,0))})
   stage=1;deadline=time.monotonic()+3;return
  if stage==1:
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherPressTab')
   stage=2;deadline=time.monotonic()+1;return
  if stage==2:
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherPressTab release')
   widgets=unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,False)
   root=[x for x in widgets if x.get_class().get_name()=='WB_TabSwitcher_C' and x.is_visible()][0]
   root.set_keyboard_focus()
   menu=root.call_method('GetTabSwitcherMenu')
   log({'initial':menu.call_method('GetActiveTabIndex'),'buttons':len(menu.get_editor_property('TabButtonsWidgets')),'initial_focus':root.has_keyboard_focus() or root.has_focused_descendants()})
   stage=3;deadline=time.monotonic()+0.3;return
  if stage==3:
   if step>=len(keys):log({'complete':True,'failures':report['failures']});finish();return
   before=menu.call_method('GetActiveTabIndex')
   expected=(before+(1 if keys[step]=='E' else -1))%len(menu.get_editor_property('TabButtonsWidgets'))
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe '+keys[step])
   stage=4;deadline=time.monotonic()+0.3;return
  if stage==4:
   actual=menu.call_method('GetActiveTabIndex')
   focused=root.has_keyboard_focus() or root.has_focused_descendants()
   passed=actual==expected and focused
   if not passed:report['failures']+=1
   log({'step':step,'key':keys[step],'expected':expected,'actual':actual,'focus_retained':focused,'pass':passed})
   step+=1;stage=3;return
 except Exception:
  report['failures']+=1;log({'error':traceback.format_exc(),'stage':stage});finish()
log({'start':True})
unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/BaseLevel')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'VTG.TabSwitcherStartPIE')
