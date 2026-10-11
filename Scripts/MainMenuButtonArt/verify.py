import unreal,pathlib
p='/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu';bp=unreal.load_asset(p)
for name in ['Button','Button_1','Button_2','Button_86']:
 b=unreal.load_object(None,p+'.WBP_MainMenu:WidgetTree.'+name);s=b.get_editor_property('widget_style')
 for f,n in zip(['normal','hovered','pressed'],['Default','Hover','Selected']):
  brush=s.get_editor_property(f);assert brush.get_editor_property('resource_object').get_name()=='MainMenuOptionBar_'+n
  assert brush.get_editor_property('draw_as')==unreal.SlateBrushDrawType.IMAGE
  assert brush.get_editor_property('tint_color').get_editor_property('specified_color')==unreal.LinearColor(1,1,1,1)
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
t=unreal.AssetExportTask();t.object=bp;t.filename=str(pathlib.Path(unreal.Paths.project_dir())/'Scripts/MainMenuButtonArt/WBP_MainMenu.after.copy');t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
unreal.log('MAINMENU_BUTTON_ART_VERIFY_PASS 4 buttons, 12 brushes')
