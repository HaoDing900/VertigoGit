import unreal
for name,path in [('Notes','/InventorySystemX/Modules/Notes/Components/AC_NotesComponent'),('Parent','/Game/LevelNSequence/BPLM')]:
 a=unreal.load_asset(path);t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/PoliceContractAudio/'+name+'.copy';t.automated=True;t.prompt=False;unreal.Exporter.run_asset_export_task(t)
