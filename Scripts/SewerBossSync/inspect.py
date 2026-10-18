import unreal,os
root=unreal.Paths.project_dir()+'Scripts/SewerBossSync/'
os.makedirs(root,exist_ok=True)
paths=['/Game/VFX/VFX_EnemyAttack/VFX_PoisonCloud/NS_PoisonCloud_Large']
for p in paths:
    a=unreal.load_asset(p)
    if not a:
        unreal.log_warning('SYNC MISSING '+p);continue
    t=unreal.AssetExportTask();t.object=a;t.filename=root+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True
    unreal.Exporter.run_asset_export_task(t)
