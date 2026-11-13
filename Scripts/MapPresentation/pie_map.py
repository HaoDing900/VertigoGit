import unreal,time,pathlib,json,traceback
folder=pathlib.Path(unreal.Paths.project_dir())/'Scripts/MapPresentation'
folder.mkdir(parents=True,exist_ok=True)
events=[];stage=0;deadline=0;start=time.monotonic()
steps=[('VTG.TabSwitcherPressTab',.1),('VTG.TabSwitcherPressTab release',1),('VTG.TabSwitcherProbe Q',.5),('VTG.MapPresentationProbe verify -1',.1),('VTG.MapPresentationProbe shot initial',.3),('VTG.MapPresentationProbe click 1',.4),('VTG.MapPresentationProbe verify 1',.1),('VTG.MapPresentationProbe shot selected',.3),('VTG.MapPresentationProbe name 1',.4),('VTG.MapPresentationProbe verify 1 description',.1),('VTG.MapPresentationProbe shot introduction',.3),('VTG.MapPresentationProbe click 2',.4),('VTG.MapPresentationProbe verify 2 description',.1),('VTG.MapPresentationProbe shot switched',.3),('VTG.MapPresentationProbe click 0',.4),('VTG.MapPresentationProbe verify 0 description',.1),('VTG.MapPresentationProbe click 3',.4),('VTG.MapPresentationProbe verify 3 description',.1),('VTG.MapPresentationProbe shot left-edge',.3),('VTG.TabSwitcherProbe E',.4),('VTG.TabSwitcherProbe E',.4),('VTG.MapPresentationProbe shot save-load',.3),('VTG.TabSwitcherProbe Tab',.4)]
def finish():
 unreal.unregister_slate_post_tick_callback(handle);unreal.EditorLevelLibrary.editor_end_play();unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'QUIT_EDITOR')
def tick(dt):
 global stage,deadline
 if time.monotonic()-start>150:events.append({'timeout':True});finish();return
 if time.monotonic()<deadline:return
 w=unreal.EditorLevelLibrary.get_game_world()
 if not w:return
 try:
  if stage==0:stage=1;deadline=time.monotonic()+3;return
  if stage>len(steps):events.append({'passed':True});(folder/'pie-result.json').write_text(json.dumps(events,indent=2));finish();return
  command,delay=steps[stage-1];unreal.SystemLibrary.execute_console_command(w,command);events.append({'command':command});(folder/'pie-result.json').write_text(json.dumps(events,indent=2));stage+=1;deadline=time.monotonic()+delay
 except Exception:events.append({'error':traceback.format_exc()});(folder/'pie-result.json').write_text(json.dumps(events,indent=2));finish()
unreal.EditorLevelLibrary.load_level('/Game/ThirdPerson/Maps/BaseLevel');handle=unreal.register_slate_post_tick_callback(tick);unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'VTG.TabSwitcherStartPIE')
