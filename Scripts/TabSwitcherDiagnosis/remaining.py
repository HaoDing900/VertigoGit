import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/TabSwitcherDiagnosis'
for p in ['/Game/Inventory/InventoryXBased/UMG/WB_NoteMainWidget','/InventorySystemX/InventorySystemX/Components/AC_Inventory_HUD_Component']:
 a=unreal.load_asset(p)
 if a:
  t=unreal.AssetExportTask();t.object=a;t.filename=str(out/(a.get_name()+'.copy'));t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
