import unreal
paths = ['/Game/Narrative/Mail/DT_Mail_ID_FillHere', '/Game/Inventory/InventoryXBased/UMG/WB_Note', '/Game/Inventory/InventoryXBased/UMG/WB_NoteMainWidget', '/Game/Inventory/InventoryXBased/UMG/WB_NoteFullScreen', '/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt', '/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1']
for path in paths:
    asset = unreal.load_asset(path)
    if not asset:
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = unreal.Paths.project_dir() + 'Scripts/MailDialogueDiagnosis/' + asset.get_name() + '.copy'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.Exporter.run_asset_export_task(task)
