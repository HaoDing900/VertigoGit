import unreal,time,pathlib,json,traceback
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/TabSwitcherDiagnosis/runtime.json'
report={'events':[],'scope':'isolated PIE widget runtime with native Slate Q/E keyboard routing'};start=time.monotonic();stage=0

def log(v):
 report['events'].append(v);out.write_text(json.dumps(report,indent=2,ensure_ascii=False),encoding='utf-8');unreal.log('TABPROBE '+str(v))
def finish():
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorLevelLibrary.editor_end_play()
 unreal.SystemLibrary.execute_console_command(unreal.EditorLevelLibrary.get_editor_world(),'QUIT_EDITOR')
def tick(delta):
 global stage,switcher,menu
 try:
  if time.monotonic()-start>100:log({'timeout':stage});finish();return
  w=unreal.EditorLevelLibrary.get_game_world()
  if not w:return
  if stage==0:
   pc=unreal.GameplayStatics.get_player_controller(w,0)
   cls=unreal.load_class(None,'/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcher.WB_TabSwitcher_C')
   switcher=unreal.get_default_object(unreal.load_class(None,'/Script/UMG.WidgetBlueprintLibrary')).call_method('Create',(w,cls,pc))
   log({'world':w.get_name(),'widget':str(switcher)})
   switcher.add_to_viewport()
   switcher.set_visibility(unreal.SlateVisibility.VISIBLE)
   switcher.call_method('ShowHideMenu',(True,))
   stage=1;return
  if stage==1:
   menu=switcher.call_method('GetTabSwitcherMenu')
   log({'menu':str(menu),'visible':menu.is_visible(),'tabs':str(menu.get_editor_property('Tabs')),'buttons':str(menu.get_editor_property('TabButtonsWidgets')),'active':str(menu.call_method('GetActiveTabIndex'))})
   log({'visible':switcher.is_visible(),'keyboard_focus':switcher.has_keyboard_focus(),'descendant_focus':switcher.has_focused_descendants()})
   log({'focus_widgets':[(x.get_name(),x.has_keyboard_focus(),x.has_focused_descendants()) for x in unreal.WidgetLibrary.get_all_widgets_of_class(w,unreal.UserWidget,False) if x.has_keyboard_focus() or x.has_focused_descendants()]})
   unreal.SystemLibrary.execute_console_command(w,'VTG.TabSwitcherProbe')
   log({'after_Slate_QE':str(menu.call_method('GetActiveTabIndex'))})
   key=unreal.Key();key.import_text('E')
   switcher.call_method('TabSwitcherInputs',(key,))
   log({'after_E':str(menu.call_method('GetActiveTabIndex'))})
   key=unreal.Key();key.import_text('Q')
   switcher.call_method('TabSwitcherInputs',(key,))
   log({'after_Q':str(menu.call_method('GetActiveTabIndex'))})
   stage=2;finish()
 except Exception:
  log({'error':traceback.format_exc(),'stage':stage});finish()
log({'api':'start'})
unreal.EditorLevelLibrary.load_level('/Engine/Maps/Entry')
handle=unreal.register_slate_post_tick_callback(tick)
unreal.EditorLevelLibrary.editor_play_simulate()



