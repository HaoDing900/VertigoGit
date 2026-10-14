import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/MapLabels'
base='/Game/Inventory/InventoryXBased/UMG/TabSwitcher/InTabWidget'
for suffix in ['/WB_TabWidget_Map','/MapWBP/WBP_Map_1','/MapWBP/WBP_Map1','/MapWBP/WBP_MapNode','/MapWBP/WBP_SubArea','/MapWBP/S_MapNodeData','/MapWBP/S_SubAreaData']:
 a=unreal.load_asset(base+suffix)
 if a:
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
