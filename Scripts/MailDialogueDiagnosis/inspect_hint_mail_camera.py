import unreal
for path in ['/Game/LevelNSequence/In_Level/SaApartment/BPLM_SaApartment','/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1','/Game/Blueprints/Widget/WBP_HintLayer','/InventorySystemX/InventorySystemX/Data/DT_KeyHints']:
    a=unreal.load_asset(path)
    if not a: continue
    t=unreal.AssetExportTask();t.object=a;t.filename=unreal.Paths.project_dir()+'Scripts/MailDialogueDiagnosis/'+a.get_name()+'.copy';t.automated=True;t.prompt=False;t.replace_identical=True;unreal.Exporter.run_asset_export_task(t)
