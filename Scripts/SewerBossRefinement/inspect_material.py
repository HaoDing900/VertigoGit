import unreal,pathlib
out=pathlib.Path(unreal.Paths.project_dir())/'Scripts/SewerBossRefinement'
a=unreal.load_asset('/InventorySystemX/InventorySystemX/Materials/Materials/Background/M_FogMadeIntoCaustic')
t=unreal.AssetExportTask();t.object=a;t.filename=str(out/'M_FogMadeIntoCaustic.t3d');t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
d=unreal.load_asset('/Game/Characters/Monster/SeaMonsterBoss/DT_SeaMonster_Phases')
unreal.DataTableFunctionLibrary.export_data_table_to_json_file(d,str(out/'phases.json'))
