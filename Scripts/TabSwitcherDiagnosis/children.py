import unreal,pathlib,json
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/TabSwitcherDiagnosis'
paths=['/Game/Inventory/InventoryXBased/UMG/Inventory/WB_Inventory','/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/WB_TabWidget','/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/WB_TabWidget_Map','/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/WB_TabWidget_Mail','/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget/WB_TabWidget_Quest','/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcher','/Game/Inventory/InventoryXBased/UMG/TabSwitcher/WB_TabSwitcherMenu']
res={}
for p in paths:
 a=unreal.load_asset(p)
 if not a:continue
 t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
 c=unreal.get_default_object(a.generated_class())
 res[p]={'focusable':str(c.get_editor_property('is_focusable'))}
 for n in ['tabs','tab_buttons_widgets']:
  try:res[p][n]=str(c.get_editor_property(n))
  except:pass
(out/'defaults.json').write_text(json.dumps(res,indent=2))
