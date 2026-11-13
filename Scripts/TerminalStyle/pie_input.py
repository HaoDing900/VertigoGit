import unreal, time, pathlib, json, traceback
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/TerminalStyle/pie-input.json'
events=[]
started=time.monotonic()
stage=0
cycle=0
deadline=started
def record(label):
    w=unreal.EditorLevelLibrary.get_game_world()
    unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe NoKey')
    roots=[x for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,False) if x.get_class().get_name()=='WB_TabSwitcher_C']
    row={'label':label,'cycle':cycle,'roots':[{'visible':x.is_visible(),'focus':x.has_keyboard_focus() or x.has_focused_descendants(),'player':str(x.get_owning_player())} for x in roots]}
    visible=[x for x in roots if x.is_visible()]
    if label in ('opened','after-E','clicked-quest'):
        assert len(visible)==1 and (visible[0].has_keyboard_focus() or visible[0].has_focused_descendants()),str(row)
    if label=='clicked-quest':
        menu=visible[0].call_method('GetTabSwitcherMenu')
        row['active']=menu.call_method('GetActiveTabIndex')
        assert row['active']==0,str(row)
    if label=='closed':assert not visible,str(row)
    events.append(row)
    out.write_text(json.dumps(events,indent=2),encoding='utf-8')
def finish():
    unreal.unregister_slate_post_tick_callback(handle)
    unreal.EditorLevelLibrary.editor_end_play()
    unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'QUIT_EDITOR')
def tick(dt):
    global stage,deadline,cycle
    if time.monotonic()-started>120: finish();return
    if time.monotonic()<deadline:return
    w=unreal.EditorLevelLibrary.get_game_world()
    if not w:return
    try:
        if stage==0:stage=1;deadline=time.monotonic()+3;return
        if stage==1:
            record('before');unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherPressTab');stage=2;deadline=time.monotonic()+.1;return
        if stage==2:
            unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherPressTab release');stage=3;deadline=time.monotonic()+1;return
        if stage==3:
            record('opened');unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe E');stage=4;deadline=time.monotonic()+.5;return
        if stage==4:
            record('after-E');unreal.SystemLibrary.execute_console_command(w,'VTG.TerminalClickQuest');stage=5;deadline=time.monotonic()+.5;return
        if stage==5:
            record('clicked-quest');unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe Tab');stage=6;deadline=time.monotonic()+.5;return
        if stage==6:
            record('closed');cycle+=1
            if cycle>=5:events.append({'passed':True,'cycles':cycle});out.write_text(json.dumps(events,indent=2));finish()
            else:stage=1;deadline=time.monotonic()+.3
    except Exception:
        events.append({'error':traceback.format_exc()});out.write_text(json.dumps(events,indent=2));finish()
unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/BaseLevel')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'VTG.TabSwitcherStartPIE')
