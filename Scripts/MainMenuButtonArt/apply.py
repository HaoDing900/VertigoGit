import unreal,pathlib,shutil,json
root=pathlib.Path(unreal.Paths.project_dir());out=root/'Scripts/MainMenuButtonArt'
p='/Game/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu';bp=unreal.load_asset(p)
textures=[unreal.load_asset('/Game/2DArt/UI/GameOption/MainMenuOptionBar_'+n) for n in ['Default','Hover','Selected']];assert all(textures)
source=root/'Content/ThirdPerson/Blueprints/UI/MainMenu/WBP_MainMenu.uasset';backup=out/'WBP_MainMenu.before_button_art.uasset';assert not backup.exists();shutil.copy2(source,backup)
report=[]
for name in ['Button','Button_1','Button_2','Button_86']:
 b=unreal.load_object(None,p+'.WBP_MainMenu:WidgetTree.'+name);assert isinstance(b,unreal.Button)
 style=b.get_editor_property('widget_style')
 for field,texture in zip(['normal','hovered','pressed'],textures):
  brush=style.get_editor_property(field)
  brush.set_editor_property('resource_object',texture)
  brush.set_editor_property('draw_as',unreal.SlateBrushDrawType.IMAGE)
  brush.set_editor_property('tint_color',unreal.SlateColor(specified_color=unreal.LinearColor(1,1,1,1)))
  style.set_editor_property(field,brush)
 b.set_editor_property('widget_style',style)
 b.set_editor_property('background_color',unreal.LinearColor(1,1,1,1))
 report.append({'button':name,'states':[str(style.get_editor_property(f).get_editor_property('resource_object').get_path_name()) for f in ['normal','hovered','pressed']]})
assert unreal.EditorAssetLibrary.save_loaded_asset(bp,only_if_is_dirty=False)
(out/'result.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('MAINMENU_BUTTON_ART_SAVE_PASS')
