import unreal
paths = ['/Game/ThirdPerson/Blueprints/BP_Player_Sa', '/Game/LevelNSequence/BPLM', '/Game/LevelNSequence/In_Level/SewerUnderApt/BPLM_SewerUnderApt', '/Game/Characters/Sa/BP_Cine_Sa', '/Game/Characters/Sa/ABP_SaCine', '/Game/Narrative/Mail/DT_Mail_ID_FillHere']
for path in paths:
    asset = unreal.load_asset(path)
    if not asset:
        continue
    task = unreal.AssetExportTask()
    task.object = asset
    task.filename = unreal.Paths.project_dir() + 'Scripts/MailDialogueDiagnosis/' + asset.get_name() + ('.json' if isinstance(asset, unreal.DataTable) else '.copy')
    task.automated = True
    task.prompt = False
    task.replace_identical = True
    unreal.Exporter.run_asset_export_task(task)
