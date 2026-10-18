import unreal
for path in ['/Game/Narrative/Dialogues/Levels/L3SaApartment_Day1/L3_SaApartmentD1', '/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt', '/Game/ThirdPerson/Blueprints/BP_Player_Sa', '/Game/LevelNSequence/BPLM', '/Game/ThirdPerson/PC_Game_VTG', '/Game/Blueprints/BP_0CameraLockPoint', '/Game/Blueprints/BP_1CameraLockAutoTrigger']:
    asset = unreal.load_asset(path)
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = unreal.Paths.project_dir() + 'Scripts/MailDialogueDiagnosis/' + asset.get_name() + '.copy'
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.Exporter.run_asset_export_task(task)
